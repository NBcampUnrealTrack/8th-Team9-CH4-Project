#include "Player/MTPlayerAttributeSet.h"

UMTPlayerAttributeSet::UMTPlayerAttributeSet()
{
	InitHp(100.f);
	InitMaxHp(100.f);
	InitStamina(100.f);
	InitMaxStamina(100.f);
}

bool UMTPlayerAttributeSet::PreGameplayEffectExecute(struct FGameplayEffectModCallbackData& Data)
{
	Super::PreGameplayEffectExecute(Data);

	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("PreGameplayEffectExecute"));

	return true;
}

void UMTPlayerAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("PreAttributeChange"));
}

void UMTPlayerAttributeSet::PostAttributeBaseChange(const FGameplayAttribute& Attribute, float OldValue,
	float NewValue) const
{
	Super::PostAttributeBaseChange(Attribute, OldValue, NewValue);

	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("PostAttributeChange"));
}

void UMTPlayerAttributeSet::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("PostGameplayEffectExecute"));
}
