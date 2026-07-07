#pragma once

#include "CoreMinimal.h"
#include "Player/MTGameplayAbility.h"
#include "GA_HeartBeam.generated.h"

class UGameplayEffect;

/** 하트광선: 5초간 카메라 전방 일자 빔(구체 스윕). 경로상 행인=매료, 적 고양이=데미지 (둘 다 초당 10). 적용 서버 권위. */
UCLASS()
class MEOWTRACTIVE_API UGA_HeartBeam : public UMTGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_HeartBeam();

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

	// 총 지속 (s)
	UPROPERTY(EditDefaultsOnly, Category = "HeartBeam", meta = (ClampMin = "0.0"))
	float Duration = 5.f;

	// 기본 사거리 (cm). 실제 = 이 값 × EyeBeamRange(패시브 배율)
	UPROPERTY(EditDefaultsOnly, Category = "HeartBeam", meta = (ClampMin = "0.0"))
	float BaseRange = 2000.f;

	// 빔 굵기 (cm) — 구체 스윕 반경
	UPROPERTY(EditDefaultsOnly, Category = "HeartBeam", meta = (ClampMin = "0.0"))
	float BeamRadius = 60.f;

	// 판정/틱 간격 (s)
	UPROPERTY(EditDefaultsOnly, Category = "HeartBeam", meta = (ClampMin = "0.01"))
	float FireInterval = 0.1f;

	// 행인 매료 초당량
	UPROPERTY(EditDefaultsOnly, Category = "HeartBeam", meta = (ClampMin = "0.0"))
	float AttractPerSecond = 10.f;

	// 적 고양이 데미지 초당량
	UPROPERTY(EditDefaultsOnly, Category = "HeartBeam", meta = (ClampMin = "0.0"))
	float DamagePerSecond = 10.f;

	// 행인 매료 GE (GA_AttractiveBeam과 동일 — SetByCaller Data.Damage)
	UPROPERTY(EditDefaultsOnly, Category = "HeartBeam")
	TSubclassOf<UGameplayEffect> AttractEffect;

	// 적 고양이 데미지 GE (GE_CatDamage — SetByCaller Data.Damage)
	UPROPERTY(EditDefaultsOnly, Category = "HeartBeam")
	TSubclassOf<UGameplayEffect> DamageEffect;

	UPROPERTY(EditDefaultsOnly, Category = "HeartBeam")
	bool bDrawDebug = true;

	// VFX 훅 (BP에서 Niagara 빔 제어)
	UFUNCTION(BlueprintImplementableEvent, Category = "HeartBeam")
	void OnBeamStart();

	UFUNCTION(BlueprintImplementableEvent, Category = "HeartBeam")
	void OnBeamEnd();

	UFUNCTION(BlueprintImplementableEvent, Category = "HeartBeam")
	void OnBeamUpdate(const FVector& Start, const FVector& End);

private:
	void FireBeam();
	void OnDurationElapsed();

	float GetEffectiveRange() const;

	FTimerHandle BeamTimerHandle;
	FTimerHandle DurationTimerHandle;
};
