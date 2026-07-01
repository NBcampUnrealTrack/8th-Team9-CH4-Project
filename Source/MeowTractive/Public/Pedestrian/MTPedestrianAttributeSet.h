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

UCLASS()
class MEOWTRACTIVE_API UMTPedestrianAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UMTPedestrianAttributeSet();

	// GE 실행값 전달용 메타 속성. 실제 플레이어별 수치는 MTAttractiveComponent가 보관한다.
	UPROPERTY(BlueprintReadOnly, Category = "Attractive")
	FGameplayAttributeData AttractiveAmount;
	ATTRIBUTE_ACCESSORS(UMTPedestrianAttributeSet, AttractiveAmount);

	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
};
