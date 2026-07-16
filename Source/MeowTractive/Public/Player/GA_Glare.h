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

	// 카메라와 플레이어 사이에 검출된 Pawn을 조준점에서 제외할 때 사용할 여유 거리 (cm)
	UPROPERTY(EditDefaultsOnly, Category = "Glare", meta = (ClampMin = "0.0"))
	float CameraPlayerDepthTolerance = 10.f;

	// 빔 시작 위치를 가져올 캐릭터 Mesh 소켓. 없으면 아바타 위치 사용.
	UPROPERTY(EditDefaultsOnly, Category = "Glare|FX")
	FName GlareSocketName = TEXT("glare");

	// 데미지 (GE에 SetByCaller Data.Damage로 주입)
	UPROPERTY(EditDefaultsOnly, Category = "Glare", meta = (ClampMin = "0.0"))
	float Damage = 20.f;

	// 슬로우 지속 (s) — GE에 SetByCaller Data.Duration으로 주입
	UPROPERTY(EditDefaultsOnly, Category = "Glare", meta = (ClampMin = "0.0"))
	float SlowDuration = 1.f;

	// 감속 곱배율 (0.7 = 30% 감속) — GE에 SetByCaller Data.SlowMult로 주입
	UPROPERTY(EditDefaultsOnly, Category = "Glare", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float SlowMultiplier = 0.7f;

	// 데미지 GE (GE_CatDamage — IncomingDamage / SetByCaller Data.Damage)
	UPROPERTY(EditDefaultsOnly, Category = "Glare")
	TSubclassOf<UGameplayEffect> DamageEffect;

	// 감속 GE (GE_MoveSlow — Duration=SetByCaller Data.Duration, Modifier: MoveSpeedMult Multiply SetByCaller Data.SlowMult)
	UPROPERTY(EditDefaultsOnly, Category = "Glare")
	TSubclassOf<UGameplayEffect> SlowEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Glare")
	bool bDrawDebug = false;

	// 연출 훅 (BP에서 눈빛 VFX/사운드). bHit=명중 여부
	UFUNCTION(BlueprintImplementableEvent, Category = "Glare")
	void OnGlare(const FVector& Start, const FVector& End, bool bHit);

private:
	// 카메라 정면 조준점을 계산한 뒤, 시작점부터 끝점까지 기존 Sphere Sweep으로 적 고양이 판정.
	AActor* FindTargetCat(FVector& OutStart, FVector& OutEnd) const;
	FVector GetGlareFXStartLocation() const;
	void ExecuteGlareGameplayCue(const FVector& Start, const FVector& End) const;

	// 패시브 배율 반영 사거리
	float GetEffectiveRange() const;
};
