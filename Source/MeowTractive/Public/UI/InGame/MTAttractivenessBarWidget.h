// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MTAttractivenessBarWidget.generated.h"

class UImage;
class UProgressBar;

UCLASS()
class MEOWTRACTIVE_API UMTAttractivenessBarWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    // 호환용 단일 바 갱신
    UFUNCTION(BlueprintCallable)
    void UpdateBar(float Current, float Max, FLinearColor TeamColor);

    // 현재 로컬 플레이어와 최고 경쟁자 수치를 겹쳐 표시
    UFUNCTION(BlueprintCallable)
    void UpdateBars(
        float Current,
        float Enemy,
        float Max,
        FLinearColor CurrentColor,
        FLinearColor EnemyColor);

protected:
	// 로컬 플레이어 매료도 바
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UProgressBar> AttractivenessBar;

    // 적 플레이어 매료도 바
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UProgressBar> EnemyAttactivenessBar;

    // 로컬 플레이어 매료도 핸들
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> CurrentAttractivenessHandle;

	// 적 플레이어 매료도 핸들
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> EnemyAttractivenessHandle;

    // false면 ProgressBar와 Image의 WBP 색상 설정을 유지.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attractiveness Bar|Appearance")
    bool bUseTeamColors = true;

private:
    float CachedCurrent = 0.f;
    float CachedEnemy = 0.f;
    float CachedMax = 100.f;
    FLinearColor CachedCurrentColor = FLinearColor::White;
    FLinearColor CachedEnemyColor = FLinearColor::White;

    void UpdateCurrentMarker(float Percent);
	void UpdateEnemyMarker(float Percent, FLinearColor Color);
};
