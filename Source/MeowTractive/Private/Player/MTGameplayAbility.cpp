#include "Player/MTGameplayAbility.h"

#include "AbilitySystemComponent.h"

const FGameplayTagContainer* UMTGameplayAbility::GetCooldownTags() const
{
	TempCooldownTags.Reset();
	if (const FGameplayTagContainer* ParentTags = Super::GetCooldownTags())
	{
		TempCooldownTags.AppendTags(*ParentTags);
	}
	TempCooldownTags.AppendTags(CooldownTags);
	return &TempCooldownTags;
}

void UMTGameplayAbility::ApplyCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	const UGameplayEffect* CooldownGE = GetCooldownGameplayEffect();
	// Cooldown GE 미지정 / 태그·지속 미설정이면 쿨다운 없음 (안전)
	if (!CooldownGE || CooldownTags.IsEmpty() || CooldownDuration <= 0.f)
	{
		return;
	}

	FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(CooldownGE->GetClass(), GetAbilityLevel());
	if (Spec.IsValid())
	{
		Spec.Data->DynamicGrantedTags.AppendTags(CooldownTags);
		Spec.Data->SetSetByCallerMagnitude(
			FGameplayTag::RequestGameplayTag(FName("Data.Cooldown")), CooldownDuration);
		ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
	}
}
