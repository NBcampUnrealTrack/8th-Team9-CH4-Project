#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "MTGameplayAbility.generated.h"

/**
 * MT 공통 어빌리티 베이스.
 * 쿨다운을 SetByCaller(Data.Cooldown)로 주입 → 지속시간을 GE가 아닌 '어빌리티에서' 튜닝.
 * 공용 Cooldown GE(Duration=SetByCaller Data.Cooldown, 정적 태그 없음) 하나를 여러 스킬이 공유하고,
 * 스킬별 쿨다운 태그(CooldownTags)는 런타임에 동적 부여한다.
 */
UCLASS(Abstract)
class MEOWTRACTIVE_API UMTGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

protected:
	// 쿨다운 지속 (s). 0이면 쿨다운 없음.
	UPROPERTY(EditDefaultsOnly, Category = "MT|Cooldown", meta = (ClampMin = "0.0"))
	float CooldownDuration = 0.f;

	// 쿨다운 중 부여/판정할 식별 태그 (Cooldown.*). 보통 파생 클래스 생성자에서 세팅.
	UPROPERTY(EditDefaultsOnly, Category = "MT|Cooldown")
	FGameplayTagContainer CooldownTags;

	// 쿨다운 태그를 GAS 쿨다운 체크에 노출
	virtual const FGameplayTagContainer* GetCooldownTags() const override;

	// SetByCaller 지속시간 + 동적 태그로 쿨다운 GE 적용
	virtual void ApplyCooldown(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

private:
	// GetCooldownTags 반환용 임시 버퍼 (상속 쿨다운 태그 + CooldownTags 병합)
	mutable FGameplayTagContainer TempCooldownTags;
};
