// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MTItemTypes.h"
#include "MTItemInventoryComponent.generated.h"

class UMTItemData;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MEOWTRACTIVE_API UMTItemInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMTItemInventoryComponent();

	// 필드 아이템이 서버에서 호출. 슬롯에 뭐가 있든 무조건 교체.
	void GrantItem(EMTItemType NewItem);

	// 소유 클라이언트가 "아이템 사용" 입력 시 호출.
	UFUNCTION(BlueprintCallable, Category = "Item")
	void RequestUseItem();

	UFUNCTION(BlueprintPure, Category = "Item")
	EMTItemType GetHeldItemType() const { return HeldItemType; }

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 본인 UI에서만 필요하므로 OwnerOnly로 리플리케이트 (대역폭 절약)
	UPROPERTY(ReplicatedUsing = OnRep_HeldItemType)
	EMTItemType HeldItemType = EMTItemType::None;

	UFUNCTION()
	void OnRep_HeldItemType();

	UFUNCTION(Server, Reliable)
	void ServerUseItem();

	UMTItemData* FindItemData(EMTItemType Type) const;

};
