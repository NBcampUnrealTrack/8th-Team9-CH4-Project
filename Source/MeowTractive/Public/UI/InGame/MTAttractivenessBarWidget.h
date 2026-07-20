// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MTAttractivenessBarWidget.generated.h"

class UImage;
class UProgressBar;
class UMaterialInterface;
class UMaterialInstanceDynamic;

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

    // 핸들 emission 머티리얼 (Class Defaults에서 할당). 미지정 시 이미지 틴트로 폴백
    UPROPERTY(EditAnywhere, Category = "Attractiveness Bar|Appearance")
    TObjectPtr<UMaterialInterface> HandleMaterial;

private:
    float CachedCurrent = 0.f;
    float CachedEnemy = 0.f;
    float CachedMax = 100.f;
    FLinearColor CachedCurrentColor = FLinearColor::White;
    FLinearColor CachedEnemyColor = FLinearColor::White;

    void UpdateCurrentMarker(float Percent);
	void UpdateEnemyMarker(float Percent, FLinearColor Color);

	// 핸들에 team color를 emission 머티리얼로 적용 (머티리얼 없으면 이미지 틴트 폴백)
	void ApplyHandleColor(UImage* Handle, TObjectPtr<UMaterialInstanceDynamic>& MID, const FLinearColor& Color);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CurrentHandleMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> EnemyHandleMID;
};
