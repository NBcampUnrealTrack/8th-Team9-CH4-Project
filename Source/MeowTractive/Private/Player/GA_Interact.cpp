#include "Player/GA_Interact.h"

#include "Player/MTLobbySelectable.h"
#include "Game/MTGameplayTags.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UGA_Interact::UGA_Interact()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// TryActivateAbilitiesByTag가 찾을 식별 태그
	SetAssetTags(FGameplayTagContainer(MTGameplayTags::Ability::TAG_Skill_Interact));

	// 액션 스킬 상호배제 + 스턴 중 발동 불가
	ActivationOwnedTags.AddTag(MTGameplayTags::State::TAG_State_Casting);
	ActivationBlockedTags.AddTag(MTGameplayTags::State::TAG_State_Casting);
	ActivationBlockedTags.AddTag(MTGameplayTags::State::TAG_State_Stun);

	HitEventTag = MTGameplayTags::Event::TAG_Event_Interact_Hit;
}

void UGA_Interact::ActivateAbility(
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

	// 시전 중 이동 정지 (종료 시 복원)
	SetRooted(true);

	// 몽타주가 있으면: 재생 + notify(GameplayEvent) 시점에 판정. 없으면 즉시 판정 후 종료.
	if (InteractMontage)
	{
		UAbilityTask_PlayMontageAndWait* MontageTask =
			UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, InteractMontage, 1.f);
		MontageTask->OnCompleted.AddDynamic(this, &UGA_Interact::OnMontageEnded);
		MontageTask->OnInterrupted.AddDynamic(this, &UGA_Interact::OnMontageEnded);
		MontageTask->OnCancelled.AddDynamic(this, &UGA_Interact::OnMontageEnded);
		MontageTask->ReadyForActivation();

		UAbilityTask_WaitGameplayEvent* EventTask =
			UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, HitEventTag, nullptr, /*OnlyTriggerOnce*/ true, /*OnlyMatchExact*/ true);
		EventTask->EventReceived.AddDynamic(this, &UGA_Interact::OnHitEvent);
		EventTask->ReadyForActivation();
	}
	else
	{
		PerformSelect();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void UGA_Interact::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	SetRooted(false);
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Interact::SetRooted(bool bNewRooted)
{
	if (bRooted == bNewRooted)
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Move)
	{
		return;
	}

	if (bNewRooted)
	{
		Move->StopMovementImmediately();
		// 공중이면 낙하는 유지 (공중 고정 방지) — 지상만 고정
		if (Move->IsMovingOnGround())
		{
			Move->SetMovementMode(MOVE_None);
		}
	}
	else if (Move->MovementMode == MOVE_None)
	{
		Move->SetMovementMode(MOVE_Walking);
	}
	bRooted = bNewRooted;
}

void UGA_Interact::OnHitEvent(FGameplayEventData Payload)
{
	PerformSelect();
}

void UGA_Interact::OnMontageEnded()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Interact::PerformSelect()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !HasAuthority(&CurrentActivationInfo))
	{
		return;
	}

	const FVector Center = Avatar->GetActorLocation() + Avatar->GetActorForwardVector() * ForwardOffset;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Avatar);
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);

	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(
		Overlaps, Center, FQuat::Identity, ObjParams, FCollisionShape::MakeSphere(Radius), Params);

	TSet<AActor*> Selected; // 중복 선택 방지
	for (const FOverlapResult& O : Overlaps)
	{
		AActor* Target = O.GetActor();
		if (!Target || Selected.Contains(Target))
		{
			continue;
		}
		if (IMTLobbySelectable* Selectable = Cast<IMTLobbySelectable>(Target))
		{
			Selected.Add(Target);
			Selectable->OnPunchSelect(Avatar);
		}
	}
}
