#pragma once

#include "CoreMinimal.h"
#include "Player/MTGameplayAbility.h"
#include "GA_HeartBeam.generated.h"

class UGameplayEffect;
class UNiagaraComponent;
class UNiagaraSystem;

/** 하트광선: 5초간 카메라 전방 일자 빔(구체 스윕)으로 경로상 행인 매료. 시전 중 이동 불가. 적용 서버 권위. */
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

	UPROPERTY(EditDefaultsOnly, Category = "HeartBeam", meta = (ClampMin = "0.0"))
	float CameraPlayerDepthTolerance = 10.f;

	// 사운드
	UPROPERTY(EditDefaultsOnly, Category = "HeartBeam|Sound")
	USoundBase* BeamLoopSound;

	UPROPERTY()
	UAudioComponent* BeamAudioComponent;

	// 판정/틱 간격 (s)
	UPROPERTY(EditDefaultsOnly, Category = "HeartBeam", meta = (ClampMin = "0.01"))
	float FireInterval = 0.1f;

	// 행인 매료 초당량
	UPROPERTY(EditDefaultsOnly, Category = "HeartBeam", meta = (ClampMin = "0.0"))
	float AttractPerSecond = 10.f;

	// 행인 매료 GE (GA_AttractiveBeam과 동일 — SetByCaller Data.Damage)
	UPROPERTY(EditDefaultsOnly, Category = "HeartBeam")
	TSubclassOf<UGameplayEffect> AttractEffect;

	UPROPERTY(EditDefaultsOnly, Category = "HeartBeam")
	bool bDrawDebug = true;

	UPROPERTY(EditDefaultsOnly, Category = "HeartBeam|FX")
	TObjectPtr<UNiagaraSystem> HeartBeamFX;

	UPROPERTY(EditDefaultsOnly, Category = "HeartBeam|FX")
	FName HeartBeamSocketName = TEXT("heartbeam");

	UPROPERTY(EditDefaultsOnly, Category = "HeartBeam|FX", meta = (ClampMin = "0.0"))
	float BeamFXFadeInTime = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "HeartBeam|FX", meta = (ClampMin = "0.0"))
	float BeamFXFadeOutTime = 0.35f;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ActiveHeartBeamFX;

	// VFX 훅 (BP에서 Niagara 빔 제어)
	UFUNCTION(BlueprintImplementableEvent, Category = "HeartBeam")
	void OnBeamStart();

	UFUNCTION(BlueprintImplementableEvent, Category = "HeartBeam")
	void OnBeamEnd();

	UFUNCTION(BlueprintImplementableEvent, Category = "HeartBeam")
	void OnBeamUpdate(const FVector& Start, const FVector& End);

private:
	void FireBeam();
	void UpdateBeamVisual();
	void OnDurationElapsed();
	void StartHeartBeamFX();
	void FinishHeartBeamFadeIn();
	void UpdateHeartBeamFX(const FVector& Start, const FVector& End);
	void StopHeartBeamFX();
	void FinishHeartBeamFX();
	FVector GetHeartBeamFXStartLocation() const;
	FLinearColor GetAvatarPlayerColor() const;
	bool TraceBeam(FVector& OutTraceStart, FVector& OutBeamEnd, bool& bOutHitWorld) const;

	float GetEffectiveRange() const;

	// 시전 중 자리고정 (해제는 EndAbility)
	void SetSelfRooted(bool bNewRooted);

	FTimerHandle BeamTimerHandle;
	FTimerHandle DurationTimerHandle;
	FTimerHandle BeamVisualTimerHandle;
	FTimerHandle BeamFXFadeInTimerHandle;
	FTimerHandle BeamFXCleanupTimerHandle;
	bool bIsBeamVisualUpdateActive = false;
	bool bIsHeartBeamFXEnding = false;
	bool bRooted = false;
};
