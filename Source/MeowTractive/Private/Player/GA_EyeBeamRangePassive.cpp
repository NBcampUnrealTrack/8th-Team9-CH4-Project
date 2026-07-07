#include "Player/GA_EyeBeamRangePassive.h"

#include "AbilitySystemComponent.h"

UGA_EyeBeamRangePassive::UGA_EyeBeamRangePassive()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// 스탯 버프는 서버 권위로만 적용 → 어트리뷰트 복제로 소유 클라에 전달
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UGA_EyeBeamRangePassive::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	// 서버에서 Grant 직후 자동 발동 (패시브)
	if (ActorInfo && ActorInfo->IsNetAuthority() && ActorInfo->AbilitySystemComponent.IsValid())
	{
		ActorInfo->AbilitySystemComponent->TryActivateAbility(Spec.Handle);
	}
}

void UGA_EyeBeamRangePassive::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !RangeEffect)
	{
		return; // 패시브는 유지 — EndAbility 호출 안 함
	}

	FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
	Ctx.AddSourceObject(GetAvatarActorFromActorInfo());
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(RangeEffect, GetAbilityLevel(), Ctx);
	if (Spec.IsValid())
	{
		AppliedHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
	// 사망/캔슬 시 EndAbility에서 GE 제거. 그 외에는 계속 유지.
}

void UGA_EyeBeamRangePassive::EndAbility(
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
