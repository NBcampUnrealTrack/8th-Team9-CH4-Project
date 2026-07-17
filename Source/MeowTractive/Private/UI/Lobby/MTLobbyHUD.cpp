#include "UI/Lobby/MTLobbyHUD.h"
#include "Game/MTLobbyGameState.h"
#include "TimerManager.h"

AMTLobbyHUD::AMTLobbyHUD()
{
	FallbackLoadingText = FText::GetEmpty();   // 로비는 문구 없이 검은 화면만
}

void AMTLobbyHUD::BeginPlay()
{
	Super::BeginPlay();

	ShowLoadingOverlay();
	ShownTime = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().SetTimer(RevealTimer, this, &AMTLobbyHUD::TryReveal, 0.2f, true);

	TryBindCountdown();
}

void AMTLobbyHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(RevealTimer);
	GetWorldTimerManager().ClearTimer(CountdownBindTimer);
	GetWorldTimerManager().ClearTimer(TravelFadeTimer);
	if (AMTLobbyGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMTLobbyGameState>() : nullptr)
	{
		GS->OnStartCountdownChanged.RemoveDynamic(this, &AMTLobbyHUD::HandleStartCountdownChanged);
	}
	Super::EndPlay(EndPlayReason);
}

void AMTLobbyHUD::TryReveal()
{
	const float Elapsed = GetWorld()->GetTimeSeconds() - ShownTime;
	const APlayerController* PC = GetOwningPlayerController();
	const bool bPawnReady = PC && PC->GetPawn() != nullptr;

	if (bPawnReady || Elapsed >= MaxDisplayTime)
	{
		GetWorldTimerManager().ClearTimer(RevealTimer);
		FadeOutLoadingOverlay(RevealFadeDuration);   // 대기 없이 바로 페이드인
	}
}

void AMTLobbyHUD::TryBindCountdown()
{
	AMTLobbyGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMTLobbyGameState>() : nullptr;
	if (!GS)
	{
		GetWorldTimerManager().SetTimer(CountdownBindTimer, this, &AMTLobbyHUD::TryBindCountdown, 0.2f, false);
		return;
	}
	GS->OnStartCountdownChanged.AddUniqueDynamic(this, &AMTLobbyHUD::HandleStartCountdownChanged);
	HandleStartCountdownChanged(GS->GetStartCountdown());   // 바인딩 전 0 도달 대비
}

void AMTLobbyHUD::HandleStartCountdownChanged(int32 RemainingSeconds)
{
	// 0 = "게임시작!" — 문구를 잠깐 보여준 뒤 검은 화면으로 페이드아웃 (서버가 그 후 트래블)
	if (RemainingSeconds != 0 || bTravelFadeStarted)
	{
		return;
	}
	bTravelFadeStarted = true;

	if (StartFadeDelay > 0.f)
	{
		GetWorldTimerManager().SetTimer(TravelFadeTimer, this, &AMTLobbyHUD::StartTravelFade, StartFadeDelay, false);
	}
	else
	{
		StartTravelFade();
	}
}

void AMTLobbyHUD::StartTravelFade()
{
	FadeInLoadingOverlay(TravelFadeDuration);
}
