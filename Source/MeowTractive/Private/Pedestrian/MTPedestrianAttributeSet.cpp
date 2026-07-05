#include "Pedestrian/MTPedestrianAttributeSet.h"

#include "Pedestrian/MTPedestrianBase.h"
#include "GameplayEffectExtension.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"

UMTPedestrianAttributeSet::UMTPedestrianAttributeSet()
{
	InitAttractiveAmount(0.f);
}

void UMTPedestrianAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	AMTPedestrianBase* Ped = Cast<AMTPedestrianBase>(GetOwningActor());
	if (!Ped)
	{
		return;
	}

	if (Data.EvaluatedData.Attribute == GetAttractiveAmountAttribute())
	{
		const float Amount = FMath::Abs(Data.EvaluatedData.Magnitude);
		SetAttractiveAmount(0.f);
		if (Amount <= 0.f)
		{
			return;
		}

		APlayerState* SourcePS = nullptr;
		if (AActor* Instigator = Data.EffectSpec.GetContext().GetOriginalInstigator())
		{
			if (const APawn* P = Cast<APawn>(Instigator))
			{
				SourcePS = P->GetPlayerState();
			}
			else if (const AController* C = Cast<AController>(Instigator))
			{
				SourcePS = C->PlayerState;
			}
		}

		if (SourcePS)
		{
			Ped->HandleAttractiveHit(SourcePS, Amount);
		}
		else
		{
			// 기존 GE_AttractiveRegen은 행인 자신이 원본 Instigator다.
			Ped->HandleAttractiveRegen(Amount);
		}
	}
}
