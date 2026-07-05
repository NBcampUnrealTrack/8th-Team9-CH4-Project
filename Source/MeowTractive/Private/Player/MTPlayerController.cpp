#include "Player/MTPlayerController.h"
#include "Player/MTPlayerState.h"
#include "UI/InGame/MTMatchGameResultWidget.h"
#include "Game/MTLobbyGameMode.h"
#include "Game/MTMatchGameMode.h"

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

void AMTPlayerController::Server_ReturnToLobby_Implementation()
{
	// 권위·트래블은 GameMode가 담당 (리슨 서버: 전원 함께 복귀)
	if (AMTMatchGameMode* Match = GetWorld() ? GetWorld()->GetAuthGameMode<AMTMatchGameMode>() : nullptr)
	{
		Match->ReturnToLobby();
	}
}

void AMTPlayerController::ClientShowResult_Implementation()
{
	// 입력 비활성화
	DisableInput(this);
	if (APawn* MyPawn = GetPawn())
    {
        MyPawn->DisableInput(this);
    }
    SetInputMode(FInputModeUIOnly());
    bShowMouseCursor = true;

    // 게임 끝 메시지
    GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Yellow, TEXT("게임 종료!"));

	// TODO: 결과 화면 위젯 생성 및 표시
	if (ResultWidgetClass)
    {
        UMTMatchGameResultWidget* ResultWidget = CreateWidget<UMTMatchGameResultWidget>(this, ResultWidgetClass);
        if (ResultWidget)
        {
            ResultWidget->AddToViewport();
            ResultWidget->ShowResult();
        }
    }
}
