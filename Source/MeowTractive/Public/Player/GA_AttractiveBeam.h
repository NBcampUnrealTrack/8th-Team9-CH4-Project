#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_AttractiveBeam.generated.h"

class UGameplayEffect;
class AMTPedestrianBase;

/** 매료빔: 누르는 동안 카메라 방향으로 '굵은 지속 빔'(구체 스윕)을 쏴 경로상 행인 매료. 적용은 서버 권위. */
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

	// 빔 굵기 (cm) — 구체 스윕 반경
	UPROPERTY(EditDefaultsOnly, Category = "AttractiveBeam", meta = (ClampMin = "0.0"))
	float BeamRadius = 40.f;

	// 행인에게 적용할 매료 데미지 GE
	UPROPERTY(EditDefaultsOnly, Category = "AttractiveBeam")
	TSubclassOf<UGameplayEffect> AttractiveDamageGE;

	// GE에 SetByCaller(Data.Damage)로 전달할 기본 데미지
	UPROPERTY(EditDefaultsOnly, Category = "AttractiveBeam", meta = (ClampMin = "0.0"))
	float BaseDamage = 10.f;

	// 디버그 라인 표시
	UPROPERTY(EditDefaultsOnly, Category = "AttractiveBeam")
	bool bDrawDebug = true;

	// 판정/틱 간격 (s)
	UPROPERTY(EditDefaultsOnly, Category = "Beam")
	float FireInterval = 0.5f;

	// --- VFX 훅 (BP에서 Niagara 빔 제어). LocalPredicted라 소유 클라/서버에서 호출 ---
	UFUNCTION(BlueprintImplementableEvent, Category = "AttractiveBeam")
	void OnBeamStart();

	UFUNCTION(BlueprintImplementableEvent, Category = "AttractiveBeam")
	void OnBeamEnd();

	// 매 틱 빔 시작/끝점 갱신 (BP가 Niagara Beam End를 여기에 맞춤)
	UFUNCTION(BlueprintImplementableEvent, Category = "AttractiveBeam")
	void OnBeamUpdate(const FVector& Start, const FVector& End, bool bHit);

private:
	// 소스(고양이) ASC로 GE 적용 → 행인 기여도가 올바르게 기록됨
	void ApplyAttractiveDamage(AMTPedestrianBase* Target);
};
