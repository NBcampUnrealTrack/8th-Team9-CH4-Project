#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Attract.generated.h"

class UGameplayEffect;
class AMTPedestrianBase;

/** 매료빔: 라인트레이스로 행인을 맞혀 GE_AttractiveDamage 적용(매료도 차감). 적용은 서버 권위. */
UCLASS()
class MEOWTRACTIVE_API UGA_Attract : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Attract();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	// 행인에게 적용할 매료 데미지 GE (GE_AttractiveDamage 지정)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attract")
	TSubclassOf<UGameplayEffect> AttractiveDamageGE;

	// 빔 사거리 (cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attract")
	float BeamRange = 1500.f;

	// 트레이스 채널 (행인 = Pawn)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attract")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Pawn;

	// 빔 시각효과 훅 (양 끝점 + 명중 여부). 디자이너가 BP에서 구현 (코스메틱).
	UFUNCTION(BlueprintImplementableEvent, Category = "Attract")
	void OnBeamFired(const FVector& Start, const FVector& End, bool bHit);

private:
	void ApplyAttractiveDamage(AMTPedestrianBase* Target);
};
