#include "Player/GA_MeowPunch.h"

#include "Player/MTPlayerCharacter.h"
#include "Player/MTLobbySelectable.h"
#include "Game/MTGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameplayTagContainer.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

UGA_MeowPunch::UGA_MeowPunch()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// 클라 예측 + 서버 판정
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// TryActivateAbilitiesByTag가 찾을 식별 태그 (BP 설정에 의존하지 않음)
	SetAssetTags(FGameplayTagContainer(MTGameplayTags::Ability::TAG_Skill_Attack_MeowPunch));

	// 스킬 상호배제: 시전 중 State.Casting 부여 + 동일 태그 시 발동 차단 (이동 스킬은 미부여라 예외)
	ActivationOwnedTags.AddTag(MTGameplayTags::State::TAG_State_Casting);
	ActivationBlockedTags.AddTag(MTGameplayTags::State::TAG_State_Casting);
	// 스킬 발동 시 진행 중인 기본공격(매료빔) 취소
	CancelAbilitiesWithTag.AddTag(MTGameplayTags::Ability::TAG_Skill_Attract_Beam);

	HitEventTag = MTGameplayTags::Event::TAG_Event_MeowPunch_Hit;
}

void UGA_MeowPunch::ActivateAbility(
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

	OnPunch(); // 연출 훅

	// 시전할 때마다 번갈아 재생 (2개면 A→B→A→B...)
	UAnimMontage* Montage = nullptr;
	ActiveMontageIndex = INDEX_NONE;
	if (PunchMontages.Num() > 0)
	{
		ActiveMontageIndex = MontageIndex % PunchMontages.Num();
		Montage = PunchMontages[ActiveMontageIndex];
		MontageIndex = (MontageIndex + 1) % PunchMontages.Num();
	}

	// 몽타주가 있으면: 재생 + notify(GameplayEvent) 시점에 판정. 없으면 즉시 판정 후 종료.
	if (Montage)
	{
		UAbilityTask_PlayMontageAndWait* MontageTask =
			UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage, 1.f);
		MontageTask->OnCompleted.AddDynamic(this, &UGA_MeowPunch::OnMontageEnded);
		MontageTask->OnInterrupted.AddDynamic(this, &UGA_MeowPunch::OnMontageEnded);
		MontageTask->OnCancelled.AddDynamic(this, &UGA_MeowPunch::OnMontageEnded);
		MontageTask->ReadyForActivation();

		UAbilityTask_WaitGameplayEvent* EventTask =
			UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, HitEventTag, nullptr, /*OnlyTriggerOnce*/ true, /*OnlyMatchExact*/ true);
		EventTask->EventReceived.AddDynamic(this, &UGA_MeowPunch::OnHitEvent);
		EventTask->ReadyForActivation();
	}
	else
	{
		PerformHit();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void UGA_MeowPunch::OnHitEvent(FGameplayEventData Payload)
{
	// 종료는 몽타주 끝에서 처리 — 여기선 데미지 창만
	PerformHit();
}

void UGA_MeowPunch::OnMontageEnded()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_MeowPunch::PerformHit()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return;
	}

	// 고양이 전방으로 밀어낸 판정 중심
	const FVector Center = Avatar->GetActorLocation() + Avatar->GetActorForwardVector() * ForwardOffset;

	// 디버그 스피어는 판정/GE와 무관하게 데미지 창에서 항상 표시 (클라 예측 포함)
	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), Center, Radius, 16, FColor::Red, false, 1.f);
	}

	// 기본공격 GameplayCue 실행
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayCueParameters CueParams;
		CueParams.Location = Avatar ? Avatar->GetActorLocation() + Avatar->GetActorForwardVector() * ForwardOffset : FVector::ZeroVector;
		CueParams.Normal = Avatar ? Avatar->GetActorForwardVector() : FVector::ForwardVector;
		CueParams.Instigator = Avatar;
		CueParams.EffectCauser = Avatar;
		CueParams.RawMagnitude = static_cast<float>(ActiveMontageIndex);
		ASC->ExecuteGameplayCue(MTGameplayTags::GameplayCue::TAG_GameplayCue_Cat_Punch, CueParams);
	}

	// 판정/데미지 적용은 서버 권위
	if (!DamageEffect || !HasAuthority(&CurrentActivationInfo))
	{
		return;
	}

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Avatar);
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);

	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(
		Overlaps, Center, FQuat::Identity, ObjParams, FCollisionShape::MakeSphere(Radius), Params);

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC)
	{
		return;
	}
	const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"));
	const FGameplayTag DurationTag = FGameplayTag::RequestGameplayTag(FName("Data.Duration"));

	TSet<AActor*> Damaged; // 중복 타격 방지
	for (const FOverlapResult& O : Overlaps)
	{
		AActor* Target = O.GetActor();
		if (!Target || Damaged.Contains(Target))
		{
			continue;
		}

		// 로비 선택 조형물: IsEnemyCat 우회 — 데미지 대신 선택 순환 (서버 권위)
		if (IMTLobbySelectable* Selectable = Cast<IMTLobbySelectable>(Target))
		{
			Damaged.Add(Target);
			Selectable->OnPunchSelect(Avatar);
			continue;
		}

		if (!AMTPlayerCharacter::IsEnemyCat(Avatar, Target))
		{
			continue;
		}

		if (AMTPlayerCharacter* TargetChar = Cast<AMTPlayerCharacter>(Target))
		{
			if (TargetChar->IsDead())
			{
				continue;
			}
		}

		Damaged.Add(Target);

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
		if (!TargetASC)
		{
			continue;
		}

		// 데미지 (GE_CatDamage가 GameplayCue.Cat.Hit로 피격 연출도 트리거)
		FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
		Ctx.AddSourceObject(Avatar);
		FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(DamageEffect, GetAbilityLevel(), Ctx);
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(DamageTag, Damage);
			SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
		}

		// 짧은 경직 (State.Stun 재사용)
		if (StunEffect)
		{
			FGameplayEffectContextHandle SCtx = SourceASC->MakeEffectContext();
			SCtx.AddSourceObject(Avatar);
			FGameplayEffectSpecHandle SSpec = SourceASC->MakeOutgoingSpec(StunEffect, GetAbilityLevel(), SCtx);
			if (SSpec.IsValid())
			{
				SSpec.Data->SetSetByCallerMagnitude(DurationTag, FlinchDuration);
				SourceASC->ApplyGameplayEffectSpecToTarget(*SSpec.Data.Get(), TargetASC);
			}
		}
	}
}
