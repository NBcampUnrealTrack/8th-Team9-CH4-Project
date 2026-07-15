#include "UI/MTLoadingOverlayHUD.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameViewportClient.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

void AMTLoadingOverlayHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveLoadingOverlay();
	Super::EndPlay(EndPlayReason);
}

void AMTLoadingOverlayHUD::ShowLoadingOverlay()
{
	if (LoadingWidget || LoadingSlateWidget.IsValid())
	{
		return;   // 이미 표시 중
	}

	if (LoadingWidgetClass)
	{
		LoadingWidget = CreateWidget<UUserWidget>(GetOwningPlayerController(), LoadingWidgetClass);
		if (LoadingWidget)
		{
			LoadingWidget->AddToViewport(100);   // 다른 HUD 위젯 위로
			return;
		}
	}

	// 폴백: 검은 배경 + 문구
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		LoadingSlateWidget =
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("BlackBrush"))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FallbackLoadingText)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 24))
			];
		Viewport->AddViewportWidgetContent(LoadingSlateWidget.ToSharedRef(), 100);
	}
}

void AMTLoadingOverlayHUD::RemoveLoadingOverlay()
{
	if (LoadingWidget)
	{
		LoadingWidget->RemoveFromParent();
		LoadingWidget = nullptr;
	}
	if (LoadingSlateWidget.IsValid())
	{
		if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(LoadingSlateWidget.ToSharedRef());
		}
		LoadingSlateWidget.Reset();
	}
}
