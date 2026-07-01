// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/InGame/MTAttractivenessBarWidget.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"

void UMTAttractivenessBarWidget::NativeConstruct()
{
    Super::NativeConstruct();

    UpdateBars(
        CachedCurrent,
        CachedEnemy,
        CachedMax,
        CachedCurrentColor,
        CachedEnemyColor);
}

//로컬 플레이어만 매료도 바 업데이트
void UMTAttractivenessBarWidget::UpdateBar(float Current, float Max, FLinearColor TeamColor)
{
    UpdateBars(Current, 0.f, Max, TeamColor, FLinearColor::White);
}

//로컬과 적 플레이어 모두 매료도 바 업데이트
void UMTAttractivenessBarWidget::UpdateBars(float Current, float Enemy, float Max, FLinearColor CurrentColor, FLinearColor EnemyColor)
{
	//캐쉬된 수치 업데이트
    CachedCurrent = Current;
    CachedEnemy = Enemy;
    CachedMax = Max;
    CachedCurrentColor = CurrentColor;
    CachedEnemyColor = EnemyColor;

	//위젯 문제있으면 종료
    if (!AttractivenessBar)
    {
        return;
    }

	//퍼센테이지 계산
    const float CurrentPercent = Max > 0.f ? FMath::Clamp(Current / Max, 0.f, 1.f) : 0.f;
    const float EnemyPercent = Max > 0.f ? FMath::Clamp(Enemy / Max, 0.f, 1.f) : 0.f;

    //적 매료도바 업데이트
    if (EnemyAttactivenessBar)
    {
        EnemyAttactivenessBar->SetPercent(EnemyPercent);
        if (bUseTeamColors)
        {
            EnemyAttactivenessBar->SetFillColorAndOpacity(EnemyColor);
        }
    }

	//로컬 매료도바 업데이트
    AttractivenessBar->SetPercent(CurrentPercent);
    if (bUseTeamColors)
    {
        AttractivenessBar->SetFillColorAndOpacity(CurrentColor);
    }

	//마커 위치 업데이트
    UpdateMarker(EnemyAttractivenessHandle, EnemyPercent, EnemyColor);
    UpdateMarker(CurrentAttractivenessHandle, CurrentPercent, CurrentColor);
}

//마커(핸들) 위치/색상 업데이트
void UMTAttractivenessBarWidget::UpdateMarker(UImage* Marker, float Percent, FLinearColor Color)
{
    if (!Marker)
    {
        return;
    }

    const float ClampedPercent = FMath::Clamp(Percent, 0.f, 1.f);
    Marker->SetVisibility(ClampedPercent > 0.f
        ? ESlateVisibility::HitTestInvisible
        : ESlateVisibility::Collapsed);
    if (bUseTeamColors)
    {
        Marker->SetColorAndOpacity(Color);
    }

    if (UCanvasPanelSlot* MarkerSlot = Cast<UCanvasPanelSlot>(Marker->Slot))
    {
        MarkerSlot->SetAnchors(FAnchors(ClampedPercent, 0.5f));
        MarkerSlot->SetAlignment(FVector2D(0.5f, 0.5f));
        MarkerSlot->SetPosition(FVector2D::ZeroVector);
    }
}
