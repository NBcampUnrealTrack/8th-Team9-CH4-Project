#include "Player/GA_Dash.h"

#include "Player/MTPlayerCharacter.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Character.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

UGA_Dash::UGA_Dash()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_Dash::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
								const FGameplayAbilityActorInfo* ActorInfo,
								const FGameplayAbilityActivationInfo ActivationInfo,
								const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FVector Direction = Character->GetActorForwardVector();

	UAbilityTask_ApplyRootMotionConstantForce* ApplyRootMotionConstantForce =
		UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
			this,
			NAME_None,
			Direction,
			DashDistance / DashDuration,					 // 거리/시간 역산 (cm/s)
			DashDuration,
			false,                                           // bIsAdditive
			nullptr,                                         // StrengthOverTime
			ERootMotionFinishVelocityMode::ClampVelocity,
			FVector::ZeroVector,
			0.f,
			bEnableGravity
		);

	ApplyRootMotionConstantForce->OnFinish.AddDynamic(this, &UGA_Dash::OnDashFinished);
	ApplyRootMotionConstantForce->ReadyForActivation();

	// 돌진 충돌 판정 — 서버에서 대시 동안 주기적으로 검사
	if (HasAuthority(&ActivationInfo) && (DamageEffect || StunEffect))
	{
		HitActors.Reset();
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				HitTimerHandle, this, &UGA_Dash::CheckDashHit, 0.02f, /*bLoop=*/true);
		}
		CheckDashHit(); // 첫 프레임 즉시
	}
}

void UGA_Dash::OnDashFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Dash::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitTimerHandle);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Dash::CheckDashHit()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return;
	}

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Avatar);
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);

	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(
		Overlaps, Avatar->GetActorLocation(), FQuat::Identity, ObjParams, FCollisionShape::MakeSphere(HitRadius), Params);

	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), Avatar->GetActorLocation(), HitRadius, 12, FColor::Cyan, false, 0.1f);
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC)
	{
		return;
	}

	const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"));
	const FGameplayTag DurationTag = FGameplayTag::RequestGameplayTag(FName("Data.Duration"));

	for (const FOverlapResult& O : Overlaps)
	{
		AActor* Target = O.GetActor();
		if (!Target || HitActors.Contains(Target))
		{
			continue;
		}
		if (!AMTPlayerCharacter::IsEnemyCat(Avatar, Target))
		{
			continue;
		}
		HitActors.Add(Target);

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
		if (!TargetASC)
		{
			continue;
		}

		// 데미지
		if (DamageEffect)
		{
			FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
			Ctx.AddSourceObject(Avatar);
			FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(DamageEffect, GetAbilityLevel(), Ctx);
			if (Spec.IsValid())
			{
				Spec.Data->SetSetByCallerMagnitude(DamageTag, HitDamage);
				SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
			}
		}

		// 스턴 (지속시간 SetByCaller 주입)
		if (StunEffect)
		{
			FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
			Ctx.AddSourceObject(Avatar);
			FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(StunEffect, GetAbilityLevel(), Ctx);
			if (Spec.IsValid())
			{
				Spec.Data->SetSetByCallerMagnitude(DurationTag, StunDuration);
				SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
			}
		}
	}
}
