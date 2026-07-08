#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffectTypes.h"
#include "GA_FattyPassive.generated.h"

class UGameplayEffect;

/** 게으르냥(패시브): Grant 즉시 무한 GE로 MaxHp +50, MoveSpeedMult ×0.7 (-30%). 현재 Hp도 +50 보정. 서버 권위. */
UCLASS()
class MEOWTRACTIVE_API UGA_FattyPassive : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_FattyPassive();

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

	// 스텟 무한 GE (GE_FattyStat — Modifier: MaxHp Add +50, MoveSpeedMult Multiply 0.7)
	UPROPERTY(EditDefaultsOnly, Category = "MT|Passive")
	TSubclassOf<UGameplayEffect> StatEffect;

	// 현재 Hp 보정량 (MaxHp 증가분만큼 채워서 시작)
	UPROPERTY(EditDefaultsOnly, Category = "MT|Passive")
	float BonusHp = 50.f;

private:
	FActiveGameplayEffectHandle AppliedHandle;
};
