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
	ATTRIBUTE_ACCESSORS(UMTPlayerAttributeSet, EyeBeamRange);
	ATTRIBUTE_ACCESSORS(UMTPlayerAttributeSet, DashCharges);
	ATTRIBUTE_ACCESSORS(UMTPlayerAttributeSet, MaxDashCharges);

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

	// 눈빛 스킬 사거리 배율 (기본 1.0). 고등어 패시브 '쳐다보냥'이 +0.5 → 1.5배
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_EyeBeamRange, Category = "MT|Stat", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData EyeBeamRange;

	// 대시 충전 (0~MaxDashCharges). Cost GE가 -1, 재충전 타이머가 +1
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DashCharges, Category = "MT|Stat", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData DashCharges;

	// 대시 최대 충전 수 (기본 3). 버프로 증가 가능
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxDashCharges, Category = "MT|Stat", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxDashCharges;

	UFUNCTION()
	void OnRep_Hp(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHp(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Stamina(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_EyeBeamRange(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_DashCharges(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxDashCharges(const FGameplayAttributeData& OldValue);
};
