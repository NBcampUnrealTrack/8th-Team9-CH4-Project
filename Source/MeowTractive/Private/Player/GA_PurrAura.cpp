#include "Player/GA_PurrAura.h"

#include "Player/MTPlayerCharacter.h"
#include "Game/MTGameplayTags.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

UGA_PurrAura::UGA_PurrAura()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 스킬 상호배제 + 기본공격 취소 (Glare와 동일 규칙). 에셋/쿨다운 태그는 BP에서 변형별 지정.
	ActivationOwnedTags.AddTag(MTGameplayTags::State::TAG_State_Casting);
	ActivationBlockedTags.AddTag(MTGameplayTags::State::TAG_State_Casting);
	CancelAbilitiesWithTag.AddTag(MTGameplayTags::Ability::TAG_Skill_Attract_Beam);
}

void UGA_PurrAura::ActivateAbility(
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

	if (bRootSelf)
	{
		SetSelfRooted(true);
	}

	// 시전 순간 1회 광역 스턴 (드러눕기) — 채널 도중 진입자는 대상 아님
	if (HasAuthority(&ActivationInfo))
	{
		ApplyStunBurst();
	}

	// 틱 루프 (첫 틱 즉시). EndAbility 시 엔진이 어빌리티 타이머를 정리하므로 채널 내 타이머는 안전.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			TickTimerHandle, this, &UGA_PurrAura::DoAuraTick, TickInterval, /*bLoop=*/true);
	}
	DoAuraTick();

	// 채널 종료 예약
	UAbilityTask_WaitDelay* Wait = UAbilityTask_WaitDelay::WaitDelay(this, Duration);
	Wait->OnFinish.AddDynamic(this, &UGA_PurrAura::OnChannelFinished);
	Wait->ReadyForActivation();
}

void UGA_PurrAura::DoAuraTick()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return;
	}

	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), Avatar->GetActorLocation(), Radius, 24, FColor::Yellow, false, TickInterval * 0.9f);
	}

	// 피해/감속은 서버 권위
	if (!HasAuthority(&CurrentActivationInfo))
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC)
	{
		return;
	}

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Avatar);
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);

	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(
		Overlaps, Avatar->GetActorLocation(), FQuat::Identity, ObjParams, FCollisionShape::MakeSphere(Radius), Params);

	const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"));
	const FGameplayTag DurationTag = FGameplayTag::RequestGameplayTag(FName("Data.Duration"));
	const FGameplayTag SlowMultTag = FGameplayTag::RequestGameplayTag(FName("Data.SlowMult"));

	for (const FOverlapResult& O : Overlaps)
	{
		AActor* Target = O.GetActor();
		if (!Target || !AMTPlayerCharacter::IsEnemyCat(Avatar, Target))
		{
			continue;
		}
		if (const AMTPlayerCharacter* TargetChar = Cast<AMTPlayerCharacter>(Target))
		{
			if (TargetChar->IsDead())
			{
				continue;
			}
		}

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
		if (!TargetASC)
		{
			continue;
		}

		if (DamageEffect && TickDamage > 0.f)
		{
			FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
			Ctx.AddSourceObject(Avatar);
			FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(DamageEffect, GetAbilityLevel(), Ctx);
			if (Spec.IsValid())
			{
				Spec.Data->SetSetByCallerMagnitude(DamageTag, TickDamage);
				SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
			}
		}

		if (SlowEffect)
		{
			FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
			Ctx.AddSourceObject(Avatar);
			FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(SlowEffect, GetAbilityLevel(), Ctx);
			if (Spec.IsValid())
			{
				Spec.Data->SetSetByCallerMagnitude(SlowMultTag, SlowMultiplier);
				Spec.Data->SetSetByCallerMagnitude(DurationTag, SlowDuration);
				SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
			}
		}
	}
}

void UGA_PurrAura::ApplyStunBurst()
{
	if (!StunEffect || StunDuration <= 0.f)
	{
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!Avatar || !SourceASC)
	{
		return;
	}

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Avatar);
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);

	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(
		Overlaps, Avatar->GetActorLocation(), FQuat::Identity, ObjParams, FCollisionShape::MakeSphere(Radius), Params);

	const FGameplayTag DurationTag = FGameplayTag::RequestGameplayTag(FName("Data.Duration"));

	TSet<AActor*> Stunned; // 캡슐 다중 오버랩 중복 방지
	for (const FOverlapResult& O : Overlaps)
	{
		AActor* Target = O.GetActor();
		if (!Target || Stunned.Contains(Target) || !AMTPlayerCharacter::IsEnemyCat(Avatar, Target))
		{
			continue;
		}
		if (const AMTPlayerCharacter* TargetChar = Cast<AMTPlayerCharacter>(Target))
		{
			if (TargetChar->IsDead())
			{
				continue;
			}
		}

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
		if (!TargetASC)
		{
			continue;
		}
		Stunned.Add(Target);

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

void UGA_PurrAura::OnChannelFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_PurrAura::SetSelfRooted(bool bNewRooted)
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}
	if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
	{
		if (bNewRooted)
		{
			Move->StopMovementImmediately();
			Move->SetMovementMode(MOVE_None);
		}
		else
		{
			Move->SetMovementMode(MOVE_Walking);
		}
	}
	bRooted = bNewRooted;
}

void UGA_PurrAura::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bRooted)
	{
		SetSelfRooted(false);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TickTimerHandle);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
