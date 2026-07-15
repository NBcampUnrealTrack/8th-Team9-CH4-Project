#include "UI/Lobby/MTLobbyHUD.h"
#include "TimerManager.h"

AMTLobbyHUD::AMTLobbyHUD()
{
	FallbackLoadingText = NSLOCTEXT("MT", "LobbyLoading", "로비로 이동 중...");
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

	if ((bPawnReady && Elapsed >= MinDisplayTime) || Elapsed >= MaxDisplayTime)
	{
		GetWorldTimerManager().ClearTimer(RevealTimer);
		RemoveLoadingOverlay();
	}
}
