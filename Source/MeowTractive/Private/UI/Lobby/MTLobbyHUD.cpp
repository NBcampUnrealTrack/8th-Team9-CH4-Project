#include "UI/Lobby/MTLobbyHUD.h"
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
}

void AMTLobbyHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(RevealTimer);
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
