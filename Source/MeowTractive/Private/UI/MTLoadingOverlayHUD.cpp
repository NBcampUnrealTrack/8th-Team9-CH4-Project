#include "UI/MTLoadingOverlayHUD.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameViewportClient.h"
#include "TimerManager.h"
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
				SAssignNew(FallbackTextBlock, STextBlock)
				.Text(FallbackLoadingText)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 24))
			];
		Viewport->AddViewportWidgetContent(LoadingSlateWidget.ToSharedRef(), 100);
	}
}

void AMTLoadingOverlayHUD::SetFallbackOverlayText(const FText& NewText)
{
	if (FallbackTextBlock.IsValid())
	{
		FallbackTextBlock->SetText(NewText);
	}
}

void AMTLoadingOverlayHUD::FadeOutLoadingOverlay(float Duration)
{
	if ((!LoadingWidget && !LoadingSlateWidget.IsValid()) ||
		GetWorldTimerManager().IsTimerActive(FadeTimer))
	{
		return;   // 표시 중 아님 또는 이미 페이드 중
	}
	if (Duration <= 0.f)
	{
		RemoveLoadingOverlay();
		return;
	}
	FadeElapsed = 0.f;
	FadeDuration = Duration;
	GetWorldTimerManager().SetTimer(FadeTimer, this, &AMTLoadingOverlayHUD::TickFade, 0.02f, true);
}

void AMTLoadingOverlayHUD::TickFade()
{
	FadeElapsed += 0.02f;
	const float Opacity = FMath::Clamp(1.f - FadeElapsed / FadeDuration, 0.f, 1.f);

	if (LoadingWidget)
	{
		LoadingWidget->SetRenderOpacity(Opacity);
	}
	if (LoadingSlateWidget.IsValid())
	{
		LoadingSlateWidget->SetRenderOpacity(Opacity);
	}

	if (Opacity <= 0.f)
	{
		RemoveLoadingOverlay();
	}
}

void AMTLoadingOverlayHUD::RemoveLoadingOverlay()
{
	GetWorldTimerManager().ClearTimer(FadeTimer);

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
		FallbackTextBlock.Reset();
	}
}
