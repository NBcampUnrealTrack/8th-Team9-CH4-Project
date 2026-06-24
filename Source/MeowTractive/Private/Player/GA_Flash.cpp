#include "Player/GA_Flash.h"

#include "Abilities/Tasks/AbilityTask_MoveToLocation.h"

void UGA_Flash::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	FVector TargetLocation = AvatarActor->GetActorForwardVector() * 500 + AvatarActor->GetActorLocation();

	UAbilityTask_MoveToLocation* MoveToLocation = UAbilityTask_MoveToLocation::MoveToLocation(
		this,
		NAME_None,
		TargetLocation,
		0.01f,
		nullptr,
		nullptr
	);

	if (MoveToLocation)
	{
		MoveToLocation->OnTargetLocationReached.AddDynamic(this, &UGA_Flash::OnMoveFinished);

		// 최종 실행
		MoveToLocation->ReadyForActivation();
	}
}

void UGA_Flash::OnMoveFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
