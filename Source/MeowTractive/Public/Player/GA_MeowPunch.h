#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "GA_MeowPunch.generated.h"

class UGameplayEffect;
class UAnimMontage;

/** 고양이 근접공격: 펀치 몽타주 재생 → AnimNotify(GameplayEvent) 시점에 '전방 적 고양이'만 데미지. 판정/적용은 서버 권위. */
UCLASS()
class MEOWTRACTIVE_API UGA_MeowPunch : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_MeowPunch();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	// 펀치 몽타주들: 2개 이상 넣으면 시전할 때마다 번갈아 재생. 미할당 시 즉시 판정.
	UPROPERTY(EditDefaultsOnly, Category = "MeowPunch")
	TArray<TObjectPtr<UAnimMontage>> PunchMontages;

	// 데미지 창을 여는 AnimNotify GameplayEvent 태그
	UPROPERTY(EditDefaultsOnly, Category = "MeowPunch")
	FGameplayTag HitEventTag;

	// 판정 반경 (cm)
	UPROPERTY(EditDefaultsOnly, Category = "MeowPunch", meta = (ClampMin = "0.0"))
	float Radius = 80.f;

	// 판정 구체를 고양이 전방으로 밀어내는 거리 (cm)
	UPROPERTY(EditDefaultsOnly, Category = "MeowPunch", meta = (ClampMin = "0.0"))
	float ForwardOffset = 80.f;

	// 데미지 (GE에 SetByCaller Data.Damage로 주입)
	UPROPERTY(EditDefaultsOnly, Category = "MeowPunch", meta = (ClampMin = "0.0"))
	float Damage = 25.f;

	// 적용할 데미지 GE (GE_CatDamage — Modifier: IncomingDamage / SetByCaller Data.Damage)
	UPROPERTY(EditDefaultsOnly, Category = "MeowPunch")
	TSubclassOf<UGameplayEffect> DamageEffect;

	// 짧은 경직 GE (Dash와 동일한 GE_Stun 재사용, 지속만 짧게). 미할당 시 경직 없음.
	UPROPERTY(EditDefaultsOnly, Category = "MeowPunch")
	TSubclassOf<UGameplayEffect> StunEffect;

	// 경직 지속 (s) — GE에 SetByCaller Data.Duration으로 주입
	UPROPERTY(EditDefaultsOnly, Category = "MeowPunch", meta = (ClampMin = "0.0"))
	float FlinchDuration = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category = "MeowPunch")
	bool bDrawDebug = true;

	// 연출 훅 (BP에서 추가 이펙트/사운드)
	UFUNCTION(BlueprintImplementableEvent, Category = "MeowPunch")
	void OnPunch();

private:
	// 전방 구체 판정 + 데미지 (서버). 디버그 스피어는 모든 머신.
	void PerformHit();

	// AnimNotify GameplayEvent 수신 → 데미지 창
	UFUNCTION()
	void OnHitEvent(FGameplayEventData Payload);

	// 몽타주 종료 → 어빌리티 종료
	UFUNCTION()
	void OnMontageEnded();

	// 번갈아 재생용 인덱스 (InstancedPerActor라 시전 간 유지)
	int32 MontageIndex = 0;
};
