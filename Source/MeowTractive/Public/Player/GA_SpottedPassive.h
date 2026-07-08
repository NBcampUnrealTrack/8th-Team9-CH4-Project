#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffectTypes.h"
#include "GA_SpottedPassive.generated.h"

class UGameplayEffect;

/** 활발냥이(패시브): Grant 즉시 무한 GE로 MaxDashCharges +1, MaxHp -20. 현재 충전도 +1 보정. 서버 권위. */
UCLASS()
class MEOWTRACTIVE_API UGA_SpottedPassive : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_SpottedPassive();

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

	// 스텟 무한 GE (GE_SpottedStat — Modifier: MaxDashCharges Add +1, MaxHp Add -20)
	UPROPERTY(EditDefaultsOnly, Category = "MT|Passive")
	TSubclassOf<UGameplayEffect> StatEffect;

private:
	FActiveGameplayEffectHandle AppliedHandle;
};
