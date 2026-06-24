#include "Player/GA_Dash.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_MoveToLocation.h"
#include "GameFramework/Character.h"

void UGA_Dash::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
								const FGameplayAbilityActorInfo* ActorInfo,
								const FGameplayAbilityActivationInfo ActivationInfo,
								const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
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
}

void UGA_Dash::OnDashFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
