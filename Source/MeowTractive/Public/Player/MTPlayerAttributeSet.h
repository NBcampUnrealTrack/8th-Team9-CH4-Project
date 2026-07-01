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
	ATTRIBUTE_ACCESSORS(UMTPlayerAttributeSet, IncomingDamage);

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Hp, Category = "MT|Stat", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Hp;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHp, Category = "MT|Stat", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxHp;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stamina, Category = "MT|Stat", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Stamina;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxStamina, Category = "MT|Stat", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxStamina;

	// 메타: 데미지 GE가 더하는 값 → PostGEExecute에서 Hp 차감 (복제 안 함)
	UPROPERTY(BlueprintReadOnly, Category = "MT|Stat", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData IncomingDamage;

	UFUNCTION()
	void OnRep_Hp(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHp(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Stamina(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);
};
