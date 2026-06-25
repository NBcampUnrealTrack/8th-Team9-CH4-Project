// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MTAttractivenessBarWidget.generated.h"

/**
 * 
 */
UCLASS()
class UMTAttractivenessBarWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 행인 C++에서 호출
    UFUNCTION(BlueprintCallable)
    void UpdateBar(float Current, float Max, FLinearColor TeamColor);

protected:
    UPROPERTY(meta = (BindWidget))
    class UProgressBar* AttractivenessBar;

};
