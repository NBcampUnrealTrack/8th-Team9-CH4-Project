#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "MTPlayerAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class MEOWTRACTIVE_API UMTPlayerAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UMTPlayerAttributeSet();

	ATTRIBUTE_ACCESSORS(UMTPlayerAttributeSet, Hp);
	ATTRIBUTE_ACCESSORS(UMTPlayerAttributeSet, MaxHp);
	ATTRIBUTE_ACCESSORS(UMTPlayerAttributeSet, Stamina);
	ATTRIBUTE_ACCESSORS(UMTPlayerAttributeSet, MaxStamina);

	virtual bool PreGameplayEffectExecute(struct FGameplayEffectModCallbackData& Data) override;

	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;

	virtual void PostAttributeBaseChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) const override;

	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

private:
	UPROPERTY()
	FGameplayAttributeData Hp;

	UPROPERTY()
	FGameplayAttributeData MaxHp;

	UPROPERTY()
	FGameplayAttributeData Stamina;

	UPROPERTY()
	FGameplayAttributeData MaxStamina;
};
