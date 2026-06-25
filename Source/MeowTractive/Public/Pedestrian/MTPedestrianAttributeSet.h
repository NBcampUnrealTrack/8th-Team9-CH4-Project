#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "MTPedestrianAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/** 행인 매료 체력 속성. HP 없음 — 매료 체력(0=매료)만 가진다. */
UCLASS()
class MEOWTRACTIVE_API UMTPedestrianAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UMTPedestrianAttributeSet();

	// 현재 매료 체력. 플레이어 바엔 (Max-Current)로 표시.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AttractiveHealth, Category = "Attractive")
	FGameplayAttributeData AttractiveHealth;
	ATTRIBUTE_ACCESSORS(UMTPedestrianAttributeSet, AttractiveHealth);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxAttractiveHealth, Category = "Attractive")
	FGameplayAttributeData MaxAttractiveHealth;
	ATTRIBUTE_ACCESSORS(UMTPedestrianAttributeSet, MaxAttractiveHealth);

	// 메타: 매료 공격 GE가 더하는 값 → PostGEExecute에서 AttractiveHealth 차감 (복제 안 함)
	UPROPERTY(BlueprintReadOnly, Category = "Attractive")
	FGameplayAttributeData IncomingAttractive;
	ATTRIBUTE_ACCESSORS(UMTPedestrianAttributeSet, IncomingAttractive);

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_AttractiveHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxAttractiveHealth(const FGameplayAttributeData& OldValue);
};
