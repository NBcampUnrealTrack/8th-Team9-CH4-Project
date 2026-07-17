#include "UI/MTLoadingOverlayHUD.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Font.h"
#include "Engine/GameViewportClient.h"
#include "TimerManager.h"
#include "Styling/CoreStyle.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

AMTLoadingOverlayHUD::AMTLoadingOverlayHUD()
{
	// WBP 다이얼로그들과 같은 서체로 통일
	static ConstructorHelpers::FObjectFinder<UFont> GumiFont(
		TEXT("/Game/Developers/Rubin/UI/Font/Gumi_Romance_Font"));
	if (GumiFont.Succeeded())
	{
		OverlayFont = GumiFont.Object;
	}
}

FSlateFontInfo AMTLoadingOverlayHUD::MakeOverlayFont(float Size) const
{
	if (!OverlayFont)
	{
		return FCoreStyle::GetDefaultFontStyle("Regular", Size);
	}
	FFontOutlineSettings Outline;
	Outline.OutlineSize = 3;
	Outline.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 1.f);
	return FSlateFontInfo(OverlayFont, Size, TEXT("Default"), Outline);
}

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
				.Font(MakeOverlayFont(24.f))
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
	bFadeToBlack = false;
	FadeElapsed = 0.f;
	FadeDuration = Duration;
	GetWorldTimerManager().SetTimer(FadeTimer, this, &AMTLoadingOverlayHUD::TickFade, 0.02f, true);
}

void AMTLoadingOverlayHUD::FadeInLoadingOverlay(float Duration)
{
	GetWorldTimerManager().ClearTimer(FadeTimer);   // 걷는 중이어도 덮는 방향이 우선 (트래블 직전)

	ShowLoadingOverlay();
	if (Duration <= 0.f)
	{
		if (LoadingWidget) { LoadingWidget->SetRenderOpacity(1.f); }
		if (LoadingSlateWidget.IsValid()) { LoadingSlateWidget->SetRenderOpacity(1.f); }
		return;
	}

	if (LoadingWidget) { LoadingWidget->SetRenderOpacity(0.f); }
	if (LoadingSlateWidget.IsValid()) { LoadingSlateWidget->SetRenderOpacity(0.f); }

	bFadeToBlack = true;
	FadeElapsed = 0.f;
	FadeDuration = Duration;
	GetWorldTimerManager().SetTimer(FadeTimer, this, &AMTLoadingOverlayHUD::TickFade, 0.02f, true);
}

void AMTLoadingOverlayHUD::TickFade()
{
	FadeElapsed += 0.02f;
	const float Alpha = FMath::Clamp(FadeElapsed / FadeDuration, 0.f, 1.f);
	const float Opacity = bFadeToBlack ? Alpha : 1.f - Alpha;

	if (LoadingWidget)
	{
		LoadingWidget->SetRenderOpacity(Opacity);
	}
	if (LoadingSlateWidget.IsValid())
	{
		LoadingSlateWidget->SetRenderOpacity(Opacity);
	}

	if (Alpha >= 1.f)
	{
		if (bFadeToBlack)
		{
			GetWorldTimerManager().ClearTimer(FadeTimer);   // 검은 화면 유지 (트래블 대기)
		}
		else
		{
			RemoveLoadingOverlay();
		}
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
