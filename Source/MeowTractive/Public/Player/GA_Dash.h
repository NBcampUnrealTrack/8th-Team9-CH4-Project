#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Dash.generated.h"

UCLASS()
class MEOWTRACTIVE_API UGA_Dash : public UGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	// 대쉬 이동 거리 (cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash")
	float DashDistance = 100.f;

	// 대쉬 지속 시간 (s)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash")
	float DashDuration = 0.1f;

	// 중력 적용 여부 (true: 낙하/경사 반영, false: 평지 칼같은 거리)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash")
	bool bEnableGravity = true;

public:
	UFUNCTION()
	void OnDashFinished();
};
