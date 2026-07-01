#include "Player/MTPlayerAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UMTPlayerAttributeSet::UMTPlayerAttributeSet()
{
	InitHp(100.f);
	InitMaxHp(100.f);
	InitStamina(100.f);
	InitMaxStamina(100.f);
}

void UMTPlayerAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Hp는 적 머리 위 체력바 등으로 모두가 볼 수 있게. 스태미나는 본인만.
	DOREPLIFETIME_CONDITION_NOTIFY(UMTPlayerAttributeSet, Hp, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMTPlayerAttributeSet, MaxHp, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMTPlayerAttributeSet, Stamina, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMTPlayerAttributeSet, MaxStamina, COND_OwnerOnly, REPNOTIFY_Always);
}

void UMTPlayerAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHpAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHp());
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxStamina());
	}
}

void UMTPlayerAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// 데미지 GE: 메타 IncomingDamage 만큼 Hp 차감
	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float LocalDamage = GetIncomingDamage();
		SetIncomingDamage(0.f);
		if (LocalDamage > 0.f)
		{
			SetHp(FMath::Clamp(GetHp() - LocalDamage, 0.f, GetMaxHp()));
			// TODO: Hp <= 0 시 기절/리스폰 처리 (미구현)
		}
	}
}

void UMTPlayerAttributeSet::OnRep_Hp(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMTPlayerAttributeSet, Hp, OldValue);
}

void UMTPlayerAttributeSet::OnRep_MaxHp(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMTPlayerAttributeSet, MaxHp, OldValue);
}

void UMTPlayerAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMTPlayerAttributeSet, Stamina, OldValue);
}

void UMTPlayerAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMTPlayerAttributeSet, MaxStamina, OldValue);
}
