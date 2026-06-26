// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/MTTypes.h"
#include "MTMatchGameResultWidget.generated.h"

UCLASS()
class MEOWTRACTIVE_API UMTMatchGameResultWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable)
    void ShowResult();

protected:
    virtual void NativeConstruct() override;

    UFUNCTION(BlueprintImplementableEvent)
    void OnResultReady(const TArray<FMTPlayerResult>& Results);
};
