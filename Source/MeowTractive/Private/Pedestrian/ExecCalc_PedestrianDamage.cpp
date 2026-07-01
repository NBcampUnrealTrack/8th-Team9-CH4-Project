// Fill out your copyright notice in the Description page of Project Settings.


#include "Pedestrian/ExecCalc_PedestrianDamage.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagContainer.h"
#include "Pedestrian/MTAttractiveComponent.h"
#include "Pedestrian/MTPedestrianBase.h"
#include "Pedestrian/MTPedestrianAttributeSet.h"

void UExecCalc_PedestrianDamage::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();

	if (!TargetASC)
	{
		return;
	}

	const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"));
	const float BaseDamage = Spec.GetSetByCallerMagnitude(DamageTag, false, 10.f);

	if (BaseDamage <= 0.0f)
	{
		return;
	}

	APlayerState* SourcePlayerState = nullptr;
	if (AActor* Instigator = Spec.GetContext().GetOriginalInstigator())
	{
		if (const APawn* Pawn = Cast<APawn>(Instigator))
		{
			SourcePlayerState = Pawn->GetPlayerState();
		}
		else if (const AController* Controller = Cast<AController>(Instigator))
		{
			SourcePlayerState = Controller->PlayerState;
		}
	}

	float HighestOtherAmount = 0.f;
	float MaxAttractiveAmount = 100.f;
	if (const AMTPedestrianBase* Pedestrian = Cast<AMTPedestrianBase>(TargetASC->GetAvatarActor()))
	{
		if (const UMTAttractiveComponent* AttractiveComponent = Pedestrian->GetAttractiveComponent())
		{
			HighestOtherAmount =
				AttractiveComponent->GetHighestAttractiveAmountExcluding(SourcePlayerState);
			MaxAttractiveAmount = AttractiveComponent->GetMaxAttractiveAmount();
		}
	}

	// 다른 플레이어 최고 매료도 0 -> 100%, 30 -> 85%, 100 -> 50%.
	const float NormalizedOtherAmount = MaxAttractiveAmount > 0.f
		? FMath::Clamp(HighestOtherAmount / MaxAttractiveAmount, 0.f, 1.f)
		: 0.f;
	const float DamageScale = FMath::Lerp(1.f, 0.5f, NormalizedOtherAmount);
	const float FinalDamage = BaseDamage * DamageScale;

	OutExecutionOutput.AddOutputModifier(
		FGameplayModifierEvaluatedData(
			UMTPedestrianAttributeSet::GetAttractiveAmountAttribute(),
			EGameplayModOp::Additive,
			FinalDamage
		)
	);
}
