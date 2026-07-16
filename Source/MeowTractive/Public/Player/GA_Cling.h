#pragma once

#include "CoreMinimal.h"
#include "Player/MTGameplayAbility.h"
#include "GA_Cling.generated.h"

class UGameplayEffect;

/** 매달리기(Q): 조준선상 적 고양이에게 도약해 매달린 뒤 0.5초 후 피해. 대상 없으면 쿨다운 미소모. */
UCLASS()
class MEOWTRACTIVE_API UGA_Cling : public UMTGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Cling();

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

	// 조준 사거리 (cm)
	UPROPERTY(EditDefaultsOnly, Category = "MT|Cling", meta = (ClampMin = "0.0"))
	float Range = 450.f;

	// 조준 스윕 반경 (cm)
	UPROPERTY(EditDefaultsOnly, Category = "MT|Cling", meta = (ClampMin = "0.0"))
	float TargetRadius = 60.f;

	// 도약 시간 (s)
	UPROPERTY(EditDefaultsOnly, Category = "MT|Cling", meta = (ClampMin = "0.05"))
	float ApproachDuration = 0.2f;

	// 매달림 유지 시간 (s) — 경과 후 피해
	UPROPERTY(EditDefaultsOnly, Category = "MT|Cling", meta = (ClampMin = "0.0"))
	float ClingDuration = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "MT|Cling", meta = (ClampMin = "0.0"))
	float Damage = 30.f;

	// 데미지 GE (GE_CatDamage — SetByCaller Data.Damage)
	UPROPERTY(EditDefaultsOnly, Category = "MT|Cling")
	TSubclassOf<UGameplayEffect> DamageEffect;

	// 시전 사운드
	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	USoundBase* ClingSound;

	UPROPERTY()
	UAudioComponent* ClingAudioComponent;

	UPROPERTY(EditDefaultsOnly, Category = "MT|Cling")
	bool bDrawDebug = false;

private:
	// 카메라 조준선상 첫 적 고양이 (Glare/Dominance와 동일 규칙, 벽 관통 X)
	AActor* FindTargetCat() const;

	UFUNCTION()
	void OnApproachFinished();

	UFUNCTION()
	void OnClingFinished();

	// 매달림 시작: 이동/충돌 정지 + 대상에 Attach (해제는 EndAbility)
	void BeginCling(AActor* Target);
	void ReleaseCling();

	TWeakObjectPtr<AActor> TargetCat;
	bool bClinging = false;
};
