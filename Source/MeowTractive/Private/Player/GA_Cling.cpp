#include "Player/GA_Cling.h"

#include "Player/MTPlayerCharacter.h"
#include "Game/MTGameplayTags.h"
#include "Game/MTLog.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"

UGA_Cling::UGA_Cling()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	SetAssetTags(FGameplayTagContainer(MTGameplayTags::Ability::TAG_Skill_Spotted_Cling));

	// 스킬 상호배제 + 기본공격 취소 (Glare와 동일 규칙)
	ActivationOwnedTags.AddTag(MTGameplayTags::State::TAG_State_Casting);
	ActivationBlockedTags.AddTag(MTGameplayTags::State::TAG_State_Casting);
	CancelAbilitiesWithTag.AddTag(MTGameplayTags::Ability::TAG_Skill_Attract_Beam);

	CooldownTags.AddTag(MTGameplayTags::Cooldown::TAG_Cooldown_Spotted_Cling);
	CooldownDuration = 8.f;
}

AActor* UGA_Cling::FindTargetCat() const
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

	const FVector Start = CamLocation;
	FVector End = Start + CamRotation.Vector() * Range;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Avatar);

	// 벽에 막히면 거기까지 (관통 없음)
	FHitResult WallHit;
	if (GetWorld()->LineTraceSingleByChannel(WallHit, Start, End, ECC_Visibility, Params))
	{
		End = WallHit.ImpactPoint;
	}

	if (bDrawDebug)
	{
		DrawDebugLine(GetWorld(), Start, End, FColor::Purple, false, 1.f, 0, 2.f);
	}

	// 조준선상 첫 폰 — 적 고양이가 아니면 실패
	FHitResult PawnHit;
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);
	if (GetWorld()->SweepSingleByObjectType(
		PawnHit, Start, End, FQuat::Identity, ObjParams, FCollisionShape::MakeSphere(TargetRadius), Params))
	{
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

void UGA_Cling::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	AActor* Target = Character ? FindTargetCat() : nullptr;

	// 대상 없으면 발동 취소 — 쿨다운 미소모
	if (!Target)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayCueParameters CueParams;
		CueParams.Location = Character->GetActorLocation();
		CueParams.Normal = Character->GetActorForwardVector();
		CueParams.Instigator = Character;
		CueParams.EffectCauser = Character;
		ASC->ExecuteGameplayCue(FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Cat.Cling.Cast")), CueParams);
	}

	TargetCat = Target;

	if (ClingSound)
	{
		if (Character)
		{
			ClingAudioComponent = UGameplayStatics::SpawnSoundAttached(
				ClingSound,
				Character->GetRootComponent(),
				NAME_None,
				FVector::ZeroVector,
				EAttachLocation::SnapToTarget,
				true // bStopWhenAttachedToDestroyed
			);
		}
	}

	// 대상에게 도약 (도착 정확도는 Attach가 보정하므로 시전 시점 방향으로 충분)
	const FVector To = Target->GetActorLocation() - Character->GetActorLocation();
	UAbilityTask_ApplyRootMotionConstantForce* Approach =
		UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
			this,
			NAME_None,
			To.GetSafeNormal(),
			To.Size() / ApproachDuration,
			ApproachDuration,
			false,
			nullptr,
			ERootMotionFinishVelocityMode::ClampVelocity,
			FVector::ZeroVector,
			0.f,
			/*bEnableGravity=*/false
		);
	Approach->OnFinish.AddDynamic(this, &UGA_Cling::OnApproachFinished);
	Approach->ReadyForActivation();
}

void UGA_Cling::OnApproachFinished()
{
	AActor* Target = TargetCat.Get();
	const AMTPlayerCharacter* TargetChar = Cast<AMTPlayerCharacter>(Target);
	if (!Target || (TargetChar && TargetChar->IsDead()))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	BeginCling(Target);

	UAbilityTask_WaitDelay* Wait = UAbilityTask_WaitDelay::WaitDelay(this, ClingDuration);
	Wait->OnFinish.AddDynamic(this, &UGA_Cling::OnClingFinished);
	Wait->ReadyForActivation();
}

void UGA_Cling::BeginCling(AActor* Target)
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}

	// Attach 이동을 CMC가 되돌리지 않게 이동 정지 + 캡슐 충돌 해제
	if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->SetMovementMode(MOVE_None);
	}
	Character->SetActorEnableCollision(false);
	Character->AttachToActor(Target, FAttachmentTransformRules::KeepWorldTransform);
	bClinging = true;

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayCueParameters CueParams;
		CueParams.Location = Target->GetActorLocation();
		CueParams.Normal = (Target->GetActorLocation() - Character->GetActorLocation()).GetSafeNormal();
		CueParams.Instigator = Character;
		CueParams.EffectCauser = Character;
		ASC->ExecuteGameplayCue(FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Cat.Cling.Impact")), CueParams);
	}
}

void UGA_Cling::ReleaseCling()
{
	if (!bClinging)
	{
		return;
	}
	bClinging = false;

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}
	Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Character->SetActorEnableCollision(true);
	if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Falling);
	}
}

void UGA_Cling::OnClingFinished()
{
	// 데미지는 서버 권위
	if (HasAuthority(&CurrentActivationInfo))
	{
		if (!DamageEffect && MTLogEnabled())
		{
			UE_LOG(LogMT, Warning, TEXT("[매달리기] DamageEffect 미지정 — BP Class Defaults 확인 필요"));
		}

		AActor* Target = TargetCat.Get();
		AMTPlayerCharacter* TargetChar = Cast<AMTPlayerCharacter>(Target);
		if (Target && (!TargetChar || !TargetChar->IsDead()) && DamageEffect)
		{
			UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
			UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
			if (SourceASC && TargetASC)
			{
				FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
				Ctx.AddSourceObject(GetAvatarActorFromActorInfo());
				FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(DamageEffect, GetAbilityLevel(), Ctx);
				if (Spec.IsValid())
				{
					Spec.Data->SetSetByCallerMagnitude(
						FGameplayTag::RequestGameplayTag(FName("Data.Damage")), Damage);
					SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
				}

				// 피해와 함께 스턴 (지속시간 SetByCaller 주입)
				if (StunEffect && StunDuration > 0.f)
				{
					FGameplayEffectContextHandle SCtx = SourceASC->MakeEffectContext();
					SCtx.AddSourceObject(GetAvatarActorFromActorInfo());
					FGameplayEffectSpecHandle SSpec = SourceASC->MakeOutgoingSpec(StunEffect, GetAbilityLevel(), SCtx);
					if (SSpec.IsValid())
					{
						SSpec.Data->SetSetByCallerMagnitude(
							FGameplayTag::RequestGameplayTag(FName("Data.Duration")), StunDuration);
						SourceASC->ApplyGameplayEffectSpecToTarget(*SSpec.Data.Get(), TargetASC);
					}
				}
			}
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Cling::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	ReleaseCling();
	TargetCat.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
