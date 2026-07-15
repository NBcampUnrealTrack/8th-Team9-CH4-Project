#include "UI/InGame/MTPlayerHUD.h"
#include "UI/InGame/MTPlayerWidget.h"
#include "Game/MTGameState.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameViewportClient.h"
#include "TimerManager.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

AMTPlayerHUD::AMTPlayerHUD()
{
	FallbackLoadingText = NSLOCTEXT("MT", "WaitingPlayers", "모든 플레이어를 기다리는 중...");
}

void AMTPlayerHUD::BeginPlay()
{
	Super::BeginPlay();

	if (PlayerWidgetClass)
	{
		PlayerWidget = CreateWidget<UMTPlayerWidget>(GetOwningPlayerController(), PlayerWidgetClass);
		if (PlayerWidget)
		{
			PlayerWidget->AddToViewport();
		}
	}

	// 매치 시작 전 = 클라 seamless 직후 스트리밍 pop-in + 전원 로딩 대기 구간 → 오버레이로 덮기
	const AGameStateBase* GS = GetWorld()->GetGameState();
	if (!GS || !GS->HasMatchStarted())
	{
		ShowLoadingOverlay();

		// 대기 문구 점 애니메이션 (끝 점은 애니메이션이 대신 그림)
		WaitingBaseText = FallbackLoadingText.ToString();
		while (WaitingBaseText.EndsWith(TEXT(".")) || WaitingBaseText.EndsWith(TEXT("…")))
		{
			WaitingBaseText.LeftChopInline(1);
		}
		WaitingDotCount = -1;
		TickWaitingDots();
		GetWorldTimerManager().SetTimer(WaitingDotsTimer, this, &AMTPlayerHUD::TickWaitingDots, 0.4f, true);

		TryBindGameState();
	}
}

void AMTPlayerHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(BindRetryTimer);
	GetWorldTimerManager().ClearTimer(WaitingDotsTimer);
	GetWorldTimerManager().ClearTimer(CountdownHideTimer);
	RemoveCountdownText();
	Super::EndPlay(EndPlayReason);
}

void AMTPlayerHUD::TryBindGameState()
{
	AMTGameState* GS = GetWorld()->GetGameState<AMTGameState>();
	if (!GS)
	{
		GetWorldTimerManager().SetTimer(BindRetryTimer, this, &AMTPlayerHUD::TryBindGameState, 0.2f, false);
		return;
	}
	GS->OnMatchStateChanged.AddUniqueDynamic(this, &AMTPlayerHUD::HandleMatchStateChanged);
	GS->OnMatchStartCountdownChanged.AddUniqueDynamic(this, &AMTPlayerHUD::HandleStartCountdownChanged);

	// 바인딩 전에 이미 진행됐으면 즉시 반영 (늦은 조인/복제 순서 대비)
	HandleStartCountdownChanged(GS->GetMatchStartCountdown());
	HandleMatchStateChanged();
}

void AMTPlayerHUD::TickWaitingDots()
{
	WaitingDotCount = (WaitingDotCount + 1) % 4;   // 0 → 1 → 2 → 3 반복
	SetFallbackOverlayText(FText::FromString(WaitingBaseText + FString::ChrN(WaitingDotCount, TEXT('.'))));
}

void AMTPlayerHUD::HandleStartCountdownChanged(int32 Value)
{
	if (Value < 0)
	{
		return;   // 비활성
	}

	// 카운트다운 첫 값 → 대기 연출 종료 + 오버레이 페이드아웃 (게임 화면 페이드인)
	if (!bCountdownShown)
	{
		bCountdownShown = true;
		GetWorldTimerManager().ClearTimer(WaitingDotsTimer);
		FadeOutLoadingOverlay(RevealFadeDuration);
	}

	if (Value > 0)
	{
		ShowCountdownText(FText::AsNumber(Value));
	}
	else
	{
		// 0 = 시작! (StartMatch와 동시) — 잠시 보여주고 제거
		ShowCountdownText(NSLOCTEXT("MT", "MatchStart", "시작!"));
		GetWorldTimerManager().SetTimer(CountdownHideTimer, this, &AMTPlayerHUD::RemoveCountdownText, 0.8f, false);
	}
}

void AMTPlayerHUD::HandleMatchStateChanged()
{
	AMTGameState* GS = GetWorld()->GetGameState<AMTGameState>();
	if (GS && GS->HasMatchStarted())
	{
		GS->OnMatchStateChanged.RemoveDynamic(this, &AMTPlayerHUD::HandleMatchStateChanged);
		GetWorldTimerManager().ClearTimer(WaitingDotsTimer);

		// 카운트다운을 못 받은 경로(값 0 복제 유실 등) 안전망 — 페이드 중이면 그대로 진행
		if (!bCountdownShown)
		{
			RemoveLoadingOverlay();
		}
	}
}

void AMTPlayerHUD::ShowCountdownText(const FText& Text)
{
	if (!CountdownContainer.IsValid())
	{
		UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
		if (!Viewport)
		{
			return;
		}
		CountdownContainer =
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SAssignNew(CountdownTextBlock, STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 96))
				.ColorAndOpacity(FLinearColor::White)
				.ShadowOffset(FVector2D(3.f, 3.f))
				.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
			];
		Viewport->AddViewportWidgetContent(CountdownContainer.ToSharedRef(), 101);   // 오버레이보다 위
	}
	if (CountdownTextBlock.IsValid())
	{
		CountdownTextBlock->SetText(Text);
	}
}

void AMTPlayerHUD::RemoveCountdownText()
{
	if (CountdownContainer.IsValid())
	{
		if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(CountdownContainer.ToSharedRef());
		}
		CountdownContainer.Reset();
		CountdownTextBlock.Reset();
	}
}
