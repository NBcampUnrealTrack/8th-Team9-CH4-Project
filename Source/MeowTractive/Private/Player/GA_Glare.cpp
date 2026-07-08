#include "Player/GA_Glare.h"

#include "Player/MTPlayerCharacter.h"
#include "Player/MTPlayerAttributeSet.h"
#include "Game/MTGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagContainer.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

UGA_Glare::UGA_Glare()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	SetAssetTags(FGameplayTagContainer(MTGameplayTags::Ability::TAG_Skill_EyeBeam_Glare));

	// 스킬 상호배제: 시전 중 State.Casting 부여 + 동일 태그 시 발동 차단 (이동 스킬은 미부여라 예외)
	ActivationOwnedTags.AddTag(MTGameplayTags::State::TAG_State_Casting);
	ActivationBlockedTags.AddTag(MTGameplayTags::State::TAG_State_Casting);
	// 스킬 발동 시 진행 중인 기본공격(매료빔) 취소
	CancelAbilitiesWithTag.AddTag(MTGameplayTags::Ability::TAG_Skill_Attract_Beam);

	// 쿨다운 (공용 Cooldown GE + 스킬별 태그). 실제 초는 BP에서 튜닝.
	CooldownTags.AddTag(MTGameplayTags::Cooldown::TAG_Cooldown_EyeBeam_Glare);
	CooldownDuration = 6.f;
}

float UGA_Glare::GetEffectiveRange() const
{
	float Mul = 1.f;
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		Mul = ASC->GetNumericAttribute(UMTPlayerAttributeSet::GetEyeBeamRangeAttribute());
	}
	return BaseRange * FMath::Max(Mul, 0.f);
}

AActor* UGA_Glare::FindTargetCat(FVector& OutStart, FVector& OutEnd) const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	const APawn* Pawn = Cast<APawn>(Avatar);
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC)
	{
		return nullptr;
	}

	FVector CamLocation;
	FRotator CamRotation;
	PC->GetPlayerViewPoint(CamLocation, CamRotation);

	OutStart = CamLocation;
	OutEnd = OutStart + CamRotation.Vector() * GetEffectiveRange();

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Avatar);

	// 벽에 막히면 거기까지 (관통 없음)
	FHitResult WallHit;
	if (GetWorld()->LineTraceSingleByChannel(WallHit, OutStart, OutEnd, ECC_Visibility, Params))
	{
		OutEnd = WallHit.ImpactPoint;
	}

	// 조준선상 첫 폰 — 적 고양이가 아니면 실패 (관통 X)
	FHitResult PawnHit;
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);
	if (GetWorld()->SweepSingleByObjectType(
		PawnHit, OutStart, OutEnd, FQuat::Identity, ObjParams, FCollisionShape::MakeSphere(TargetRadius), Params))
	{
		OutEnd = PawnHit.ImpactPoint;
		AActor* Target = PawnHit.GetActor();
		if (AMTPlayerCharacter::IsEnemyCat(Avatar, Target))
		{
			if (const AMTPlayerCharacter* TargetChar = Cast<AMTPlayerCharacter>(Target))
			{
				if (TargetChar->IsDead())
				{
					return nullptr;
				}
			}
			return Target;
		}
	}
	return nullptr;
}

void UGA_Glare::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	AActor* Target = FindTargetCat(Start, End);
	const bool bHit = (Target != nullptr);

	if (bDrawDebug)
	{
		DrawDebugLine(GetWorld(), Start, End, bHit ? FColor::Red : FColor::Green, false, 0.5f, 0, 2.f);
	}

	OnGlare(Start, End, bHit); // 연출 훅 (예측 클라/서버 모두)

	// 데미지/슬로우 적용은 서버 권위
	if (bHit && HasAuthority(&ActivationInfo))
	{
		UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
		if (SourceASC && TargetASC)
		{
			const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"));
			const FGameplayTag DurationTag = FGameplayTag::RequestGameplayTag(FName("Data.Duration"));

			if (DamageEffect)
			{
				FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
				Ctx.AddSourceObject(GetAvatarActorFromActorInfo());
				FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(DamageEffect, GetAbilityLevel(), Ctx);
				if (Spec.IsValid())
				{
					Spec.Data->SetSetByCallerMagnitude(DamageTag, Damage);
					SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
				}
			}

			if (SlowEffect)
			{
				FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
				Ctx.AddSourceObject(GetAvatarActorFromActorInfo());
				FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(SlowEffect, GetAbilityLevel(), Ctx);
				if (Spec.IsValid())
				{
					Spec.Data->SetSetByCallerMagnitude(DurationTag, SlowDuration);
					Spec.Data->SetSetByCallerMagnitude(
						FGameplayTag::RequestGameplayTag(FName("Data.SlowMult")), SlowMultiplier);
					SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
				}
			}
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
