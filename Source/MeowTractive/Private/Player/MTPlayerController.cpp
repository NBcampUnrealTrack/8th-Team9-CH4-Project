#include "Player/MTPlayerController.h"
#include "Player/MTPlayerState.h"
#include "Game/MTLobbyGameMode.h"

void AMTPlayerController::Server_SetSelectedCat_Implementation(EMTCatType NewCat)
{
	AMTPlayerState* MTPS = GetPlayerState<AMTPlayerState>();
	if (!MTPS || MTPS->IsReady())   // 준비 상태에선 변경 금지
	{
		return;
	}
	MTPS->SetSelectedCat(NewCat);
}

void AMTPlayerController::Server_ToggleReady_Implementation()
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (Now - LastReadyToggleTime < ReadyCooldown)   // 0.5s 쿨다운
	{
		return;
	}
	LastReadyToggleTime = Now;

	if (AMTPlayerState* MTPS = GetPlayerState<AMTPlayerState>())
	{
		MTPS->SetReady(!MTPS->IsReady());
	}
}

void AMTPlayerController::Server_RequestStartMatch_Implementation()
{
	// 시작 권한·조건 검증은 GameMode가 담당
	if (AMTLobbyGameMode* Lobby = GetWorld() ? GetWorld()->GetAuthGameMode<AMTLobbyGameMode>() : nullptr)
	{
		Lobby->TryStartMatch();
	}
}
