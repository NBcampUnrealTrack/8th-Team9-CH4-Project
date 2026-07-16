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

    // 로컬 플레이어가 호스트(리슨 서버)인지 — "로비로 나가기" 버튼 활성 조건
    UFUNCTION(BlueprintPure, Category = "MT|UI")
    bool IsHost() const;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UFUNCTION(BlueprintImplementableEvent)
    void OnResultReady(const TArray<FMTPlayerResult>& Results);

private:
    // seamless travel은 뷰포트 위젯을 자동 제거하지 않음 → 월드 종료 시 직접 제거 (다음 판 잔존 방지)
    void HandleWorldTearDown(UWorld* World);

    FDelegateHandle WorldTearDownHandle;
};
