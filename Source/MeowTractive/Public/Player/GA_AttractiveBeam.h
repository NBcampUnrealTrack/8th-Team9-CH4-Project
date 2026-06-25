#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_AttractiveBeam.generated.h"

class UGameplayEffect;
class AMTPedestrianBase;

/** 매료빔: 카메라 라인트레이스로 행인을 맞혀 GE_AttractiveDamage 적용(매료도 차감). 적용은 서버 권위. */
UCLASS()
class MEOWTRACTIVE_API UGA_AttractiveBeam : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_AttractiveBeam();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	// 빔 사거리 (cm)
	UPROPERTY(EditDefaultsOnly, Category = "AttractiveBeam")
	float TraceDistance = 2000.f;

	// 행인에게 적용할 매료 데미지 GE (GE_AttractiveDamage 지정)
	UPROPERTY(EditDefaultsOnly, Category = "AttractiveBeam")
	TSubclassOf<UGameplayEffect> AttractiveDamageGE;

	// 디버그 라인 표시
	UPROPERTY(EditDefaultsOnly, Category = "AttractiveBeam")
	bool bDrawDebug = true;

private:
	// 소스(고양이) ASC로 GE 적용 → 행인 기여도가 올바르게 기록됨
	void ApplyAttractiveDamage(AMTPedestrianBase* Target);
};
