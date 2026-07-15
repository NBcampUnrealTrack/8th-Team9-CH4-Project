#include "UI/InGame/MTPlayerHUD.h"
#include "UI/InGame/MTPlayerWidget.h"
#include "Game/MTGameState.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"

AMTPlayerHUD::AMTPlayerHUD()
{
	FallbackLoadingText = NSLOCTEXT("MT", "WaitingPlayers", "다른 플레이어를 기다리는 중...");
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
		TryBindMatchState();
	}
}

void AMTPlayerHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(BindRetryTimer);
	Super::EndPlay(EndPlayReason);
}

void AMTPlayerHUD::TryBindMatchState()
{
	AMTGameState* GS = GetWorld()->GetGameState<AMTGameState>();
	if (!GS)
	{
		GetWorldTimerManager().SetTimer(BindRetryTimer, this, &AMTPlayerHUD::TryBindMatchState, 0.2f, false);
		return;
	}
	GS->OnMatchStateChanged.AddUniqueDynamic(this, &AMTPlayerHUD::HandleMatchStateChanged);
	HandleMatchStateChanged();   // 바인딩 전에 이미 시작됐으면 즉시 제거
}

void AMTPlayerHUD::HandleMatchStateChanged()
{
	AMTGameState* GS = GetWorld()->GetGameState<AMTGameState>();
	if (GS && GS->HasMatchStarted())
	{
		GS->OnMatchStateChanged.RemoveDynamic(this, &AMTPlayerHUD::HandleMatchStateChanged);
		RemoveLoadingOverlay();
	}
}
