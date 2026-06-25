#include "Pedestrian/MTPedestrianAttributeSet.h"

#include "Pedestrian/MTPedestrianBase.h"
#include "GameplayEffectExtension.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

UMTPedestrianAttributeSet::UMTPedestrianAttributeSet()
{
	InitMaxAttractiveHealth(100.f);
	InitAttractiveHealth(100.f);
}

void UMTPedestrianAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// 모든 클라가 동일한 매료 체력을 봐야 함
	DOREPLIFETIME_CONDITION_NOTIFY(UMTPedestrianAttributeSet, AttractiveHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMTPedestrianAttributeSet, MaxAttractiveHealth, COND_None, REPNOTIFY_Always);
}

void UMTPedestrianAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetAttractiveHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxAttractiveHealth());
	}
}

void UMTPedestrianAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	AMTPedestrianBase* Ped = Cast<AMTPedestrianBase>(GetOwningActor());
	if (!Ped)
	{
		return;
	}

	// 1) 매료 공격: 메타 IncomingAttractive 만큼 AttractiveHealth 차감 + 기여도 기록
	if (Data.EvaluatedData.Attribute == GetIncomingAttractiveAttribute())
	{
		const float Local = GetIncomingAttractive();
		SetIncomingAttractive(0.f);
		if (Local <= 0.f)
		{
			return;
		}

		// 매료한 고양이 추출 
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

		SetAttractiveHealth(FMath::Clamp(GetAttractiveHealth() - Local, 0.f, GetMaxAttractiveHealth()));
		Ped->HandleAttractiveHit(SourcePS, Local, GetAttractiveHealth());
	}
	// 2) 회복 GE: AttractiveHealth 직접 증가 → 만탱 도달 시 기여도 리셋
	else if (Data.EvaluatedData.Attribute == GetAttractiveHealthAttribute())
	{
		SetAttractiveHealth(FMath::Clamp(GetAttractiveHealth(), 0.f, GetMaxAttractiveHealth()));
		Ped->HandleAttractiveRegen(GetAttractiveHealth());
	}
}

void UMTPedestrianAttributeSet::OnRep_AttractiveHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMTPedestrianAttributeSet, AttractiveHealth, OldValue);
	// 클라: 복제 수신 시 이벤트 기반 바 갱신
	if (AMTPedestrianBase* Ped = Cast<AMTPedestrianBase>(GetOwningActor()))
	{
		Ped->BroadcastAttractiveChanged();
	}
}

void UMTPedestrianAttributeSet::OnRep_MaxAttractiveHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMTPedestrianAttributeSet, MaxAttractiveHealth, OldValue);
	if (AMTPedestrianBase* Ped = Cast<AMTPedestrianBase>(GetOwningActor()))
	{
		Ped->BroadcastAttractiveChanged();
	}
}
