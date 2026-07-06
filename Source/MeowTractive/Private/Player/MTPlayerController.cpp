#include "Player/MTPlayerController.h"
#include "Player/MTPlayerState.h"
#include "UI/InGame/MTMatchGameResultWidget.h"
#include "Game/MTLobbyGameMode.h"
#include "Game/MTMatchGameMode.h"
#include "Game/MTGameInstance.h"

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

void AMTPlayerController::Server_UpdateRoomSettings_Implementation(FMTRoomSettings NewSettings)
{
	// 호스트 검증·적용은 GameMode가 담당
	if (AMTLobbyGameMode* Lobby = GetWorld() ? GetWorld()->GetAuthGameMode<AMTLobbyGameMode>() : nullptr)
	{
		Lobby->ApplyRoomSettings(this, NewSettings);
	}
}

void AMTPlayerController::Server_KickPlayer_Implementation(APlayerState* TargetPlayer)
{
	if (AMTLobbyGameMode* Lobby = GetWorld() ? GetWorld()->GetAuthGameMode<AMTLobbyGameMode>() : nullptr)
	{
		Lobby->KickPlayer(this, TargetPlayer);
	}
}

void AMTPlayerController::ClientWasKicked_Implementation(const FText& KickReason)
{
	// 메인메뉴 복귀 시 MessageDialog가 이 사유를 표시 (ConsumeDisconnectMessage)
	if (UMTGameInstance* GI = GetGameInstance<UMTGameInstance>())
	{
		GI->SetPendingDisconnectMessage(KickReason.IsEmpty()
			? FText::FromString(TEXT("방장에 의해 강퇴되었습니다."))
			: KickReason);
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
