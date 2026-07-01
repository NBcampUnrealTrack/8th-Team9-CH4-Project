#include "Player/MTPlayerAttributeSet.h"

#include "Game/MTLog.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"

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
			const float OldHp = GetHp();
			SetHp(FMath::Clamp(OldHp - LocalDamage, 0.f, GetMaxHp()));
			const float NewHp = GetHp();

			if (MTLogEnabled())
			{
				const AActor* Attacker = Data.EffectSpec.GetContext().GetOriginalInstigator();
				const FString Msg = FString::Printf(
					TEXT("[MTDamage] %s ← %.0f 데미지 (가해:%s) | Hp %.0f → %.0f"),
					*GetNameSafe(GetOwningActor()), LocalDamage, *GetNameSafe(Attacker), OldHp, NewHp);
				UE_LOG(LogMT, Log, TEXT("%s"), *Msg);
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, Msg);

				if (NewHp <= 0.f)
				{
					const FString Dead = FString::Printf(
						TEXT("[MTDamage] %s Hp 0 도달 → 사망/기절/리스폰 미구현"), *GetNameSafe(GetOwningActor()));
					UE_LOG(LogMT, Warning, TEXT("%s"), *Dead);
					if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, Dead);
				}
			}

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
