#pragma once

#include "CoreMinimal.h"
#include "Player/MTGameplayAbility.h"
#include "GA_Glare.generated.h"

class UGameplayEffect;

/** 째려보기: 카메라 조준선상 사거리 내 '첫 적 고양이 1명'에게 즉발 데미지 + 이동속도 슬로우. 판정/적용 서버 권위. */
UCLASS()
class MEOWTRACTIVE_API UGA_Glare : public UMTGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Glare();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	// 기본 사거리 (cm). 실제 사거리 = 이 값 × EyeBeamRange(패시브 배율)
	UPROPERTY(EditDefaultsOnly, Category = "Glare", meta = (ClampMin = "0.0"))
	float BaseRange = 1500.f;

	// 조준 보정용 구체 반경 (cm)
	UPROPERTY(EditDefaultsOnly, Category = "Glare", meta = (ClampMin = "0.0"))
	float TargetRadius = 40.f;

	// 데미지 (GE에 SetByCaller Data.Damage로 주입)
	UPROPERTY(EditDefaultsOnly, Category = "Glare", meta = (ClampMin = "0.0"))
	float Damage = 20.f;

	// 슬로우 지속 (s) — GE에 SetByCaller Data.Duration으로 주입
	UPROPERTY(EditDefaultsOnly, Category = "Glare", meta = (ClampMin = "0.0"))
	float SlowDuration = 1.f;

	// 데미지 GE (GE_CatDamage — IncomingDamage / SetByCaller Data.Damage)
	UPROPERTY(EditDefaultsOnly, Category = "Glare")
	TSubclassOf<UGameplayEffect> DamageEffect;

	// 슬로우 GE (GE_Slow — grants State.Slow, Duration = SetByCaller Data.Duration)
	UPROPERTY(EditDefaultsOnly, Category = "Glare")
	TSubclassOf<UGameplayEffect> SlowEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Glare")
	bool bDrawDebug = false;

	// 연출 훅 (BP에서 눈빛 VFX/사운드). bHit=명중 여부
	UFUNCTION(BlueprintImplementableEvent, Category = "Glare")
	void OnGlare(const FVector& Start, const FVector& End, bool bHit);

private:
	// 카메라 조준선상 첫 적 고양이 탐색 (없으면 nullptr)
	AActor* FindTargetCat(FVector& OutStart, FVector& OutEnd) const;

	// 패시브 배율 반영 사거리
	float GetEffectiveRange() const;
};
