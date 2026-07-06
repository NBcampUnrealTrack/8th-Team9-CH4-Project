// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MTItemTypes.h"
#include "MTItemData.generated.h"

class UGameplayEffect;
class UNiagaraSystem;
class UStaticMesh;
/**
 *
 */
UCLASS()
class MEOWTRACTIVE_API UMTItemData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Item")
	EMTItemType ItemType = EMTItemType::None;

	UPROPERTY(EditDefaultsOnly, Category = "Item")
	TSubclassOf<UGameplayEffect> EffectToApply;

	UPROPERTY(EditDefaultsOnly, Category = "Item|Visual")
	UStaticMesh* ItemMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Item|Visual")
	UTexture2D* Icon = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Item|FX")
	UNiagaraSystem* PickupFX = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Item|FX")
	USoundBase* PickupSound = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Item|FX")
	UNiagaraSystem* UseFX = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Item|FX")
	USoundBase* UseSound = nullptr;
};
