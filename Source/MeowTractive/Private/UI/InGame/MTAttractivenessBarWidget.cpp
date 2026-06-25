// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/InGame/MTAttractivenessBarWidget.h"
#include "Components/ProgressBar.h"

void UMTAttractivenessBarWidget::UpdateBar(float Current, float Max, FLinearColor TeamColor)
{
    if (!AttractivenessBar) return;

    AttractivenessBar->SetPercent(Max > 0.f ? Current / Max : 0.f);
    AttractivenessBar->SetFillColorAndOpacity(TeamColor);
}
