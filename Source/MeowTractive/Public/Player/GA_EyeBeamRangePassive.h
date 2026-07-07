#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffectTypes.h"
#include "GA_EyeBeamRangePassive.generated.h"

class UGameplayEffect;

/** 쳐다보냥(패시브): Grant 즉시 무한 GE로 EyeBeamRange +0.5 → 눈빛 사거리 1.5배. 서버 권위. */
UCLASS()
class MEOWTRACTIVE_API UGA_EyeBeamRangePassive : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_EyeBeamRangePassive();

protected:
	// Grant 시 자동 활성화 (서버)
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	// EyeBeamRange를 올리는 무한 GE (GE_EyeBeamRange — Modifier: EyeBeamRange Add 0.5)
	UPROPERTY(EditDefaultsOnly, Category = "EyeBeam|Passive")
	TSubclassOf<UGameplayEffect> RangeEffect;

private:
	FActiveGameplayEffectHandle AppliedHandle;
};
