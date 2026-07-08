#include "Player/GA_SpottedPassive.h"

#include "Player/MTPlayerAttributeSet.h"
#include "AbilitySystemComponent.h"

UGA_SpottedPassive::UGA_SpottedPassive()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// 스탯 버프는 서버 권위로만 적용 → 어트리뷰트 복제로 소유 클라에 전달
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UGA_SpottedPassive::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	// 서버에서 Grant 직후 자동 발동 (패시브)
	if (ActorInfo && ActorInfo->IsNetAuthority() && ActorInfo->AbilitySystemComponent.IsValid())
	{
		ActorInfo->AbilitySystemComponent->TryActivateAbility(Spec.Handle);
	}
}

void UGA_SpottedPassive::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !StatEffect)
	{
		return; // 패시브는 유지 — EndAbility 호출 안 함
	}

	FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
	Ctx.AddSourceObject(GetAvatarActorFromActorInfo());
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(StatEffect, GetAbilityLevel(), Ctx);
	if (Spec.IsValid())
	{
		AppliedHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}

	// MaxDashCharges는 GE가 올리지만 현재 충전은 자동으로 안 차므로 즉시 +1 (3/4 → 4/4)
	const FGameplayAttribute ChargeAttr = UMTPlayerAttributeSet::GetDashChargesAttribute();
	ASC->SetNumericAttributeBase(ChargeAttr, ASC->GetNumericAttribute(ChargeAttr) + 1.f);
}

void UGA_SpottedPassive::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (AppliedHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveActiveGameplayEffect(AppliedHandle);
		}
		AppliedHandle.Invalidate();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
