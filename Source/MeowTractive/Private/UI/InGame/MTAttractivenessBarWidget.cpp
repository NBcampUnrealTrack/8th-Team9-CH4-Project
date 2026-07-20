// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/InGame/MTAttractivenessBarWidget.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"

// Widget 생성 시 캐시된 현재 플레이어와 경쟁자 매료도를 즉시 반영한다.
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

	//핸들 위치 업데이트
    UpdateEnemyMarker(EnemyPercent, EnemyColor);
    UpdateCurrentMarker(CurrentPercent);
}

//마커(핸들) 위치/색상 업데이트
void UMTAttractivenessBarWidget::UpdateCurrentMarker(float Percent)
{
    if (!CurrentAttractivenessHandle)
    {
        return;
    }

	// 0 이상일때만 활성화
    const float ClampedPercent = FMath::Clamp(Percent, 0.f, 1.f);
	CurrentAttractivenessHandle->SetVisibility(ClampedPercent > 0.f
        ? ESlateVisibility::HitTestInvisible
        : ESlateVisibility::Collapsed);
	if (bUseTeamColors)
	{
		ApplyHandleColor(CurrentAttractivenessHandle, CurrentHandleMID, CachedCurrentColor);
	}

	//앵커 위치를 옮겨서 마커 이동(비율에 따른 이동 구현)
    if (UCanvasPanelSlot* MarkerSlot = Cast<UCanvasPanelSlot>(CurrentAttractivenessHandle->Slot))
    {
        MarkerSlot->SetAnchors(FAnchors(ClampedPercent, 0.0f));
    }
}

void UMTAttractivenessBarWidget::UpdateEnemyMarker(float Percent, FLinearColor Color)
{
	if (!EnemyAttractivenessHandle)
	{
		return;
	}

	// 0 이상일때만 활성화
	const float ClampedPercent = FMath::Clamp(Percent, 0.f, 1.f);
	EnemyAttractivenessHandle->SetVisibility(ClampedPercent > 0.f
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed);
	if (bUseTeamColors)
	{
		ApplyHandleColor(EnemyAttractivenessHandle, EnemyHandleMID, Color);
	}

	//앵커 위치를 옮겨서 마커 이동(비율에 따른 이동 구현)
	if (UCanvasPanelSlot* MarkerSlot = Cast<UCanvasPanelSlot>(EnemyAttractivenessHandle->Slot))
	{
		MarkerSlot->SetAnchors(FAnchors(ClampedPercent, 0.0f));
	}
}

//핸들 team color를 emission 머티리얼로 적용 (없으면 이미지 틴트 폴백)
void UMTAttractivenessBarWidget::ApplyHandleColor(UImage* Handle, TObjectPtr<UMaterialInstanceDynamic>& MID, const FLinearColor& Color)
{
	if (!Handle)
	{
		return;
	}

	if (HandleMaterial)
	{
		if (!MID)
		{
			MID = UMaterialInstanceDynamic::Create(HandleMaterial, this);
			// WBP에 설정된 원본 텍스처를 그대로 파라미터로 넘겨 이미지 유지
			if (UTexture2D* Tex = Cast<UTexture2D>(Handle->GetBrush().GetResourceObject()))
			{
				MID->SetTextureParameterValue(TEXT("HandleTex"), Tex);
			}
			Handle->SetBrushFromMaterial(MID);
		}
		MID->SetVectorParameterValue(TEXT("TeamColor"), Color);
	}
	else
	{
		Handle->SetColorAndOpacity(Color);
	}
}
