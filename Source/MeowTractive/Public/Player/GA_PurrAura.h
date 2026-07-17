#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Player/MTGameplayAbility.h"
#include "GA_PurrAura.generated.h"

class UGameplayEffect;
class UAnimMontage;

/**
 * 뚱냥이 골골 오라 채널링 공용 클래스 — BP 두 개로 변형 구성.
 * 골골거리기(Q): 4초, 700cm, 초당 5피해 + 20% 감속 3초 (기본값)
 * 드러눕기(E): 시전 순간 2초 광역 스턴 + 4초 채널 초당 20피해, bRootSelf=true (BP에서 설정, 감속 없음)
 * 쿨다운 태그/초는 BP에서 지정 (Cooldown.Fatty.Purr / Cooldown.Fatty.LieDown).
 */
UCLASS()
class MEOWTRACTIVE_API UGA_PurrAura : public UMTGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_PurrAura();

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

	// 시전 애니메이션 (BP 변형별 지정: 드러눕기/골골). 채널 종료·취소 시 자동 정지.
	UPROPERTY(EditDefaultsOnly, Category = "MT|Purr")
	TObjectPtr<UAnimMontage> CastMontage;

	// 시전 사운드
	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	USoundBase* PurrLoopSound;

	UPROPERTY()
	UAudioComponent* PurrAudioComponent;

	// 오라 반경 (cm)
	UPROPERTY(EditDefaultsOnly, Category = "MT|Purr", meta = (ClampMin = "0.0"))
	float Radius = 700.f;

	// 채널 지속 (s)
	UPROPERTY(EditDefaultsOnly, Category = "MT|Purr", meta = (ClampMin = "0.1"))
	float Duration = 4.f;

	// 틱 간격 (s)
	UPROPERTY(EditDefaultsOnly, Category = "MT|Purr", meta = (ClampMin = "0.1"))
	float TickInterval = 1.f;

	// 틱당 피해
	UPROPERTY(EditDefaultsOnly, Category = "MT|Purr", meta = (ClampMin = "0.0"))
	float TickDamage = 5.f;

	// 틱당 행인 매료량 (이중 판정 — 반경 내 행인에겐 피해 대신 매료). 0이면 매료 없음
	UPROPERTY(EditDefaultsOnly, Category = "MT|Purr", meta = (ClampMin = "0.0"))
	float TickAttractiveDamage = 5.f;

	// 행인 매료 GE (GE_AttractiveDamage — SetByCaller Data.Damage, 매료빔과 동일 에셋)
	UPROPERTY(EditDefaultsOnly, Category = "MT|Purr")
	TSubclassOf<UGameplayEffect> AttractiveDamageGE;

	// 감속 곱배율 (0.8 = 20% 감속, 0.5 = 50% 감속)
	UPROPERTY(EditDefaultsOnly, Category = "MT|Purr", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float SlowMultiplier = 0.8f;

	// 감속 지속 (s). 채널 중 틱마다 갱신됨
	UPROPERTY(EditDefaultsOnly, Category = "MT|Purr", meta = (ClampMin = "0.0"))
	float SlowDuration = 3.f;

	// true면 채널 동안 자기 이동 불가 (드러눕기)
	UPROPERTY(EditDefaultsOnly, Category = "MT|Purr")
	bool bRootSelf = false;

	// 나이아가라 GameplayCue 태그. 미지정 시 Purr/LieDown을 bRootSelf 기준으로 자동 선택.
	UPROPERTY(EditDefaultsOnly, Category = "MT|Purr|FX")
	FGameplayTag GameplayCueTag;

	// 데미지 GE (GE_CatDamage — SetByCaller Data.Damage)
	UPROPERTY(EditDefaultsOnly, Category = "MT|Purr")
	TSubclassOf<UGameplayEffect> DamageEffect;

	// 감속 GE (GE_MoveSlow — Duration=SetByCaller Data.Duration, Modifier: MoveSpeedMult Multiply SetByCaller Data.SlowMult)
	UPROPERTY(EditDefaultsOnly, Category = "MT|Purr")
	TSubclassOf<UGameplayEffect> SlowEffect;

	// 시전 순간 1회 광역 스턴 지속 (s). 0 또는 StunEffect 미지정이면 스턴 없음. 채널 도중 진입자는 스턴 안 걸림.
	UPROPERTY(EditDefaultsOnly, Category = "MT|Purr", meta = (ClampMin = "0.0"))
	float StunDuration = 0.f;

	// 스턴 GE (GE_Stun — Duration=SetByCaller Data.Duration, MeowPunch/Dash와 동일 에셋 재사용)
	UPROPERTY(EditDefaultsOnly, Category = "MT|Purr")
	TSubclassOf<UGameplayEffect> StunEffect;

	UPROPERTY(EditDefaultsOnly, Category = "MT|Purr")
	bool bDrawDebug = false;

private:
	// 반경 내 적 고양이에게 틱 피해 + 감속 (서버). 연출은 양쪽.
	void DoAuraTick();

	// 시전 순간 반경 내 적 전원 스턴 1회 (서버)
	void ApplyStunBurst();

	UFUNCTION()
	void OnChannelFinished();

	void SetSelfRooted(bool bRooted);

	FTimerHandle TickTimerHandle;
	FGameplayTag ActiveGameplayCueTag;
	bool bRooted = false;
};
