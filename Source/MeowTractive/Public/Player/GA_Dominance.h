#pragma once

#include "CoreMinimal.h"
#include "Player/MTGameplayAbility.h"
#include "GA_Dominance.generated.h"

class UGameplayEffect;

/** 서열정리(E): 전방 600cm 돌진, 부딪힌 첫 적에게 피해 후 정지. 피해 직전 대상 체력이 처형선 이하면 즉사. 미스여도 쿨다운 소모. */
UCLASS()
class MEOWTRACTIVE_API UGA_Dominance : public UMTGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Dominance();

protected:
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

	// 돌진 거리 (cm)
	UPROPERTY(EditDefaultsOnly, Category = "MT|Dominance", meta = (ClampMin = "0.0"))
	float DashDistance = 600.f;

	// 돌진 시간 (s)
	UPROPERTY(EditDefaultsOnly, Category = "MT|Dominance", meta = (ClampMin = "0.05"))
	float DashDuration = 0.25f;

	// 충돌 판정 반경 (cm)
	UPROPERTY(EditDefaultsOnly, Category = "MT|Dominance", meta = (ClampMin = "0.0"))
	float HitRadius = 80.f;

	UPROPERTY(EditDefaultsOnly, Category = "MT|Dominance", meta = (ClampMin = "0.0"))
	float Damage = 20.f;

	// 처형선: 피해 직전 대상 체력이 이하면 즉사
	UPROPERTY(EditDefaultsOnly, Category = "MT|Dominance", meta = (ClampMin = "0.0"))
	float ExecuteThreshold = 30.f;

	// 데미지 GE (GE_CatDamage — SetByCaller Data.Damage)
	UPROPERTY(EditDefaultsOnly, Category = "MT|Dominance")
	TSubclassOf<UGameplayEffect> DamageEffect;

	UPROPERTY(EditDefaultsOnly, Category = "MT|Dominance")
	bool bDrawDebug = false;

private:
	// 돌진 중 주기 충돌 검사 (서버) — 첫 적 명중 시 피해/처형 후 돌진 종료
	void CheckDashHit();

	UFUNCTION()
	void OnDashFinished();

	FTimerHandle HitTimerHandle;
};
