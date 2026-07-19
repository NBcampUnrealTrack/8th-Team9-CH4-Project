#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "GA_Interact.generated.h"

class UAnimMontage;

/** 로비 상호작용(F): 몽타주 재생 → AnimNotify(GameplayEvent) 시점에 전방 IMTLobbySelectable 선택. 전투 요소 없음. */
UCLASS()
class MEOWTRACTIVE_API UGA_Interact : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Interact();

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
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	// 상호작용 몽타주. 미할당 시 즉시 판정.
	UPROPERTY(EditDefaultsOnly, Category = "Interact")
	TObjectPtr<UAnimMontage> InteractMontage;

	// 판정 창을 여는 AnimNotify GameplayEvent 태그
	UPROPERTY(EditDefaultsOnly, Category = "Interact")
	FGameplayTag HitEventTag;

	// 판정 반경 (cm)
	UPROPERTY(EditDefaultsOnly, Category = "Interact", meta = (ClampMin = "0.0"))
	float Radius = 80.f;

	// 판정 구체를 고양이 전방으로 밀어내는 거리 (cm)
	UPROPERTY(EditDefaultsOnly, Category = "Interact", meta = (ClampMin = "0.0"))
	float ForwardOffset = 80.f;

private:
	// 전방 구체 판정 → IMTLobbySelectable::OnPunchSelect (서버 권위)
	void PerformSelect();

	// 시전 중 이동 정지/복원 (PurrAura 루팅 패턴)
	void SetRooted(bool bNewRooted);

	// AnimNotify GameplayEvent 수신 → 판정 창
	UFUNCTION()
	void OnHitEvent(FGameplayEventData Payload);

	// 몽타주 종료 → 어빌리티 종료
	UFUNCTION()
	void OnMontageEnded();

	bool bRooted = false;
};
