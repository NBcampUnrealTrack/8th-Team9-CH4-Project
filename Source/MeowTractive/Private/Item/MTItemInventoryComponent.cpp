// Fill out your copyright notice in the Description page of Project Settings.


#include "Item/MTItemInventoryComponent.h"
#include "Item/MTItemData.h"
#include "Game/MTGameState.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerState.h"

// Sets default values for this component's properties
UMTItemInventoryComponent::UMTItemInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	// ...
}

void UMTItemInventoryComponent::GrantItem(EMTItemType NewItem)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	// 기존에 뭐가 있었든 그냥 덮어씀 (교체)
	HeldItemType = NewItem;

	// 서버 로컬(리슨 서버)에서는 OnRep이 자동 호출되지 않으므로 직접 호출
	OnRep_HeldItemType();
}

void UMTItemInventoryComponent::RequestUseItem()
{
	ServerUseItem();
}

void UMTItemInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UMTItemInventoryComponent, HeldItemType, COND_OwnerOnly);
}

void UMTItemInventoryComponent::OnRep_HeldItemType()
{
	// UI 바인딩용 델리게이트가 있다면 여기서 Broadcast
}

void UMTItemInventoryComponent::ServerUseItem_Implementation()
{
	if (HeldItemType == EMTItemType::None) return;

	UMTItemData* Data = FindItemData(HeldItemType);
	if (!Data || !Data->EffectToApply)
	{
		HeldItemType = EMTItemType::None;
		OnRep_HeldItemType();
		return;
	}

	AActor* Owner = GetOwner();
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Owner);
	if (!ASI)
	{
		if (APlayerState* PS = Cast<APlayerState>(Owner))
		{
			ASI = Cast<IAbilitySystemInterface>(PS->GetPawn());
		}
	}

	if (ASI && ASI->GetAbilitySystemComponent())
	{
		UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
		FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
		Ctx.AddSourceObject(this);
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(Data->EffectToApply, 1.f, Ctx);
		if (Spec.IsValid())
		{
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}

	HeldItemType = EMTItemType::None;
	OnRep_HeldItemType();
}

UMTItemData* UMTItemInventoryComponent::FindItemData(EMTItemType Type) const
{
	if (UWorld* World = GetWorld())
	{
		if (AMTGameState* GS = World->GetGameState<AMTGameState>())
		{
			return GS->GetItemData(Type);
		}
	}
	return nullptr;
}



