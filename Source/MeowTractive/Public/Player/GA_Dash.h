#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Dash.generated.h"

class UGameplayEffect;

UCLASS()
class MEOWTRACTIVE_API UGA_Dash : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Dash();

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

	// 대쉬 이동 거리 (cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash")
	float DashDistance = 100.f;

	// 대쉬 지속 시간 (s)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash")
	float DashDuration = 0.1f;

	// 중력 적용 여부 (true: 낙하/경사 반영, false: 평지 칼같은 거리)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash")
	bool bEnableGravity = true;

	// --- 돌진 충돌 (적 고양이) ---
	// 충돌 판정 반경 (cm)
	UPROPERTY(EditDefaultsOnly, Category = "Dash|Hit", meta = (ClampMin = "0.0"))
	float HitRadius = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "Dash|Hit", meta = (ClampMin = "0.0"))
	float HitDamage = 40.f;

	// 스턴 지속 (s) — GE에 SetByCaller Data.Duration으로 주입
	UPROPERTY(EditDefaultsOnly, Category = "Dash|Hit", meta = (ClampMin = "0.0"))
	float StunDuration = 2.f;

	// 데미지 GE (GE_CatDamage — SetByCaller Data.Damage)
	UPROPERTY(EditDefaultsOnly, Category = "Dash|Hit")
	TSubclassOf<UGameplayEffect> DamageEffect;

	// 스턴 GE (GE_Stun — grants State.Stun, Duration = SetByCaller Data.Duration)
	UPROPERTY(EditDefaultsOnly, Category = "Dash|Hit")
	TSubclassOf<UGameplayEffect> StunEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Dash|Hit")
	bool bDrawDebug = false;

public:
	UFUNCTION()
	void OnDashFinished();

private:
	// 대시 중 주기 충돌 판정 (서버)
	void CheckDashHit();

	FTimerHandle HitTimerHandle;

	// 이번 대시에 이미 맞은 대상 (중복 방지). 서버 전용, GC 대비 약참조.
	TSet<TWeakObjectPtr<AActor>> HitActors;
};
