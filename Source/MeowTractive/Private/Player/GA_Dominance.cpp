#include "Player/GA_Dominance.h"

#include "Player/MTPlayerCharacter.h"
#include "Player/MTPlayerAttributeSet.h"
#include "Game/MTGameplayTags.h"
#include "Game/MTLog.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Character.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

UGA_Dominance::UGA_Dominance()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	SetAssetTags(FGameplayTagContainer(MTGameplayTags::Ability::TAG_Skill_Spotted_Dominance));

	// 스킬 상호배제 + 기본공격 취소 (Glare와 동일 규칙)
	ActivationOwnedTags.AddTag(MTGameplayTags::State::TAG_State_Casting);
	ActivationBlockedTags.AddTag(MTGameplayTags::State::TAG_State_Casting);
	CancelAbilitiesWithTag.AddTag(MTGameplayTags::Ability::TAG_Skill_Attract_Beam);

	CooldownTags.AddTag(MTGameplayTags::Cooldown::TAG_Cooldown_Spotted_Dominance);
	CooldownDuration = 12.f;
}

void UGA_Dominance::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
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
		ASC->ExecuteGameplayCue(FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Cat.Dominance.Cast")), CueParams);
	}

	// 전방 돌진 (타겟 없이 발동 — 명중 판정은 돌진 중 CheckDashHit)
	UAbilityTask_ApplyRootMotionConstantForce* Lunge =
		UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
			this,
			NAME_None,
			Character->GetActorForwardVector(),
			DashDistance / DashDuration,
			DashDuration,
			false,
			nullptr,
			ERootMotionFinishVelocityMode::ClampVelocity,
			FVector::ZeroVector,
			0.f,
			/*bEnableGravity=*/false
		);
	Lunge->OnFinish.AddDynamic(this, &UGA_Dominance::OnDashFinished);
	Lunge->ReadyForActivation();

	// 돌진 충돌 판정 — 서버에서 돌진 동안 주기적으로 검사 (Dash와 동일 패턴)
	if (HasAuthority(&ActivationInfo))
	{
		if (!DamageEffect && MTLogEnabled())
		{
			UE_LOG(LogMT, Warning, TEXT("[서열정리] DamageEffect 미지정 — BP Class Defaults 확인 필요"));
		}
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				HitTimerHandle, this, &UGA_Dominance::CheckDashHit, 0.02f, /*bLoop=*/true);
		}
		CheckDashHit(); // 첫 프레임 즉시
	}
}

void UGA_Dominance::CheckDashHit()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!Avatar || !SourceASC || !DamageEffect)
	{
		return;
	}

	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), Avatar->GetActorLocation(), HitRadius, 12, FColor::Orange, false, 0.1f);
	}

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Avatar);
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);

	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(
		Overlaps, Avatar->GetActorLocation(), FQuat::Identity, ObjParams, FCollisionShape::MakeSphere(HitRadius), Params);

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

		FGameplayCueParameters CueParams;
		CueParams.Location = Target->GetActorLocation();
		CueParams.Normal = (Target->GetActorLocation() - Avatar->GetActorLocation()).GetSafeNormal();
		CueParams.Instigator = Avatar;
		CueParams.EffectCauser = Avatar;
		SourceASC->ExecuteGameplayCue(FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Cat.Dominance.Impact")), CueParams);

		// 처형 판정: 피해 직전 체력이 처형선 이하면 즉사 피해
		const float TargetHp = TargetASC->GetNumericAttribute(UMTPlayerAttributeSet::GetHpAttribute());
		const bool bExecute = (TargetHp <= ExecuteThreshold);
		const float FinalDamage = bExecute ? TargetHp : Damage;

		if (bExecute && MTLogEnabled())
		{
			UE_LOG(LogMT, Log, TEXT("[서열정리] 처형! %s (Hp %.0f <= %.0f)"),
				*GetNameSafe(Target), TargetHp, ExecuteThreshold);
		}

		FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
		Ctx.AddSourceObject(Avatar);
		FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(DamageEffect, GetAbilityLevel(), Ctx);
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(
				FGameplayTag::RequestGameplayTag(FName("Data.Damage")), FinalDamage);
			SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
		}

		// 명중 시 스턴 (처형이면 사망 처리라 생략)
		if (!bExecute && StunEffect && StunDuration > 0.f)
		{
			FGameplayEffectContextHandle SCtx = SourceASC->MakeEffectContext();
			SCtx.AddSourceObject(Avatar);
			FGameplayEffectSpecHandle SSpec = SourceASC->MakeOutgoingSpec(StunEffect, GetAbilityLevel(), SCtx);
			if (SSpec.IsValid())
			{
				SSpec.Data->SetSetByCallerMagnitude(
					FGameplayTag::RequestGameplayTag(FName("Data.Duration")), StunDuration);
				SourceASC->ApplyGameplayEffectSpecToTarget(*SSpec.Data.Get(), TargetASC);
			}
		}

		// 첫 명중에서 돌진 종료 (단일 대상)
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}
}

void UGA_Dominance::OnDashFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Dominance::EndAbility(
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
