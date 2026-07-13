#include "Player/GA_HeartBeam.h"

#include "Player/MTPlayerCharacter.h"
#include "Player/MTPlayerAttributeSet.h"
#include "Pedestrian/MTPedestrianBase.h"
#include "Game/MTGameplayTags.h"
#include "Game/MTLog.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagContainer.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

UGA_HeartBeam::UGA_HeartBeam()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	SetAssetTags(FGameplayTagContainer(MTGameplayTags::Ability::TAG_Skill_EyeBeam_HeartBeam));

	// 스킬 상호배제: 시전 중 State.Casting 부여 + 동일 태그 시 발동 차단 (이동 스킬은 미부여라 예외)
	ActivationOwnedTags.AddTag(MTGameplayTags::State::TAG_State_Casting);
	ActivationBlockedTags.AddTag(MTGameplayTags::State::TAG_State_Casting);
	// 스킬 발동 시 진행 중인 기본공격(매료빔) 취소
	CancelAbilitiesWithTag.AddTag(MTGameplayTags::Ability::TAG_Skill_Attract_Beam);

	// 쿨다운 (공용 Cooldown GE + 스킬별 태그). 실제 초는 BP에서 튜닝.
	CooldownTags.AddTag(MTGameplayTags::Cooldown::TAG_Cooldown_EyeBeam_HeartBeam);
	CooldownDuration = 12.f;
}

float UGA_HeartBeam::GetEffectiveRange() const
{
	float Mul = 1.f;
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		Mul = ASC->GetNumericAttribute(UMTPlayerAttributeSet::GetEyeBeamRangeAttribute());
	}
	return BaseRange * FMath::Max(Mul, 0.f);
}

void UGA_HeartBeam::ActivateAbility(
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

	OnBeamStart();

	FireBeam(); // 첫 발 즉시

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			BeamTimerHandle, this, &UGA_HeartBeam::FireBeam, FireInterval, /*bLoop=*/true);
		// 5초 후 자동 종료
		World->GetTimerManager().SetTimer(
			DurationTimerHandle, this, &UGA_HeartBeam::OnDurationElapsed, Duration, /*bLoop=*/false);
	}
}

void UGA_HeartBeam::OnDurationElapsed()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_HeartBeam::FireBeam()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	const APawn* Pawn = Cast<APawn>(Avatar);
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC)
	{
		return;
	}

	FVector CamLocation;
	FRotator CamRotation;
	PC->GetPlayerViewPoint(CamLocation, CamRotation);
	const FVector Start = CamLocation;
	const FVector TraceEnd = Start + CamRotation.Vector() * GetEffectiveRange();

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Avatar);

	// 벽(가시성)에 막히면 빔은 거기까지
	FVector BeamEnd = TraceEnd;
	FHitResult WallHit;
	if (GetWorld()->LineTraceSingleByChannel(WallHit, Start, TraceEnd, ECC_Visibility, Params))
	{
		BeamEnd = WallHit.ImpactPoint;
	}

	if (bDrawDebug)
	{
		DrawDebugLine(GetWorld(), Start, BeamEnd, FColor::Magenta, false, FireInterval, 0, 3.f);
	}

	OnBeamUpdate(Start, BeamEnd); // VFX 끝점 갱신

	// 적용은 서버 권위. 경로상 '전원'(관통) 판정.
	if (!HasAuthority(&CurrentActivationInfo))
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC)
	{
		return;
	}

	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);
	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByObjectType(
		Hits, Start, BeamEnd, FQuat::Identity, ObjParams, FCollisionShape::MakeSphere(BeamRadius), Params);

	const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"));
	const float AttractTick = AttractPerSecond * FireInterval;
	const float DamageTick = DamagePerSecond * FireInterval;

	TSet<AActor*> Processed; // 스윕 다중 히트 중복 방지
	for (const FHitResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		if (!Target || Processed.Contains(Target))
		{
			continue;
		}
		Processed.Add(Target);

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
		if (!TargetASC)
		{
			continue;
		}

		// 행인 → 매료
		if (Target->IsA(AMTPedestrianBase::StaticClass()))
		{
			if (AttractEffect && AttractTick > 0.f)
			{
				FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
				Ctx.AddSourceObject(Avatar);
				FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(AttractEffect, GetAbilityLevel(), Ctx);
				if (Spec.IsValid())
				{
					Spec.Data->SetSetByCallerMagnitude(DamageTag, AttractTick);
					SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
				}
			}
			continue;
		}

		// 적 고양이 → 데미지
		if (AMTPlayerCharacter::IsEnemyCat(Avatar, Target))
		{
			if (const AMTPlayerCharacter* TargetChar = Cast<AMTPlayerCharacter>(Target))
			{
				if (TargetChar->IsDead())
				{
					continue;
				}
			}
			if (DamageEffect && DamageTick > 0.f)
			{
				FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
				Ctx.AddSourceObject(Avatar);
				FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(DamageEffect, GetAbilityLevel(), Ctx);
				if (Spec.IsValid())
				{
					Spec.Data->SetSetByCallerMagnitude(DamageTag, DamageTick);
					SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
				}
			}
		}
	}
}

void UGA_HeartBeam::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BeamTimerHandle);
		World->GetTimerManager().ClearTimer(DurationTimerHandle);
	}

	OnBeamEnd();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
