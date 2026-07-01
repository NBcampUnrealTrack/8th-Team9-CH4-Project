#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_AttractiveBeam.generated.h"

class UGameplayEffect;
class AMTPedestrianBase;

/** 매료빔: 카메라 라인트레이스로 행인을 맞혀 플레이어별 매료도를 올린다. 적용은 서버 권위. */
UCLASS()
class MEOWTRACTIVE_API UGA_AttractiveBeam : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_AttractiveBeam();

protected:
	FTimerHandle BeamTimerHandle;

	void FireBeam();

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

	// 빔 사거리 (cm)
	UPROPERTY(EditDefaultsOnly, Category = "AttractiveBeam")
	float TraceDistance = 2000.f;

	// 행인에게 적용할 매료 데미지 GE
	UPROPERTY(EditDefaultsOnly, Category = "AttractiveBeam")
	TSubclassOf<UGameplayEffect> AttractiveDamageGE;

	// ExecCalc에 SetByCaller(Data.Damage)로 전달할 기본 데미지
	UPROPERTY(EditDefaultsOnly, Category = "AttractiveBeam", meta = (ClampMin = "0.0"))
	float BaseDamage = 10.f;

	// 디버그 라인 표시
	UPROPERTY(EditDefaultsOnly, Category = "AttractiveBeam")
	bool bDrawDebug = true;

	UPROPERTY(EditDefaultsOnly, Category = "Beam")
	float FireInterval = 0.1f;

private:
	// 소스(고양이) ASC로 GE 적용 → 행인 기여도가 올바르게 기록됨
	void ApplyAttractiveDamage(AMTPedestrianBase* Target);
};
