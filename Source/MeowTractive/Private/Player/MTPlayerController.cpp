#include "Player/MTPlayerController.h"
#include "Player/MTPlayerState.h"
#include "UI/InGame/MTMatchGameResultWidget.h"
#include "UI/PauseMenu/MTPauseMenuWidget.h"
#include "Game/MTLobbyGameMode.h"
#include "Game/MTMatchGameMode.h"
#include "Game/MTGameInstance.h"
#include "InputCoreTypes.h"

void AMTPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// ESC → 일시정지 메뉴 토글 (로컬 PC에만 InputComponent 존재)
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AMTPlayerController::TogglePauseMenu);
	}
}

void AMTPlayerController::TogglePauseMenu()
{
	if (!IsLocalController())
	{
		return;
	}

	// 열려 있으면 닫기 → 게임 입력 복원
	if (PauseMenu)
	{
		PauseMenu->RemoveFromParent();
		PauseMenu = nullptr;
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
		return;
	}

	// 닫혀 있으면 열기 → UIOnly로 이동 차단 + 커서
	if (!PauseMenuClass)
	{
		return;
	}
	PauseMenu = CreateWidget<UMTPauseMenuWidget>(this, PauseMenuClass);
	if (!PauseMenu)
	{
		return;
	}
	PauseMenu->AddToViewport(50);

	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(PauseMenu->TakeWidget());   // 위젯이 ESC 키 수신
	SetInputMode(Mode);
	SetShowMouseCursor(true);
}

void AMTPlayerController::Server_SetSelectedCat_Implementation(EMTCatType NewCat)
{
	AMTPlayerState* MTPS = GetPlayerState<AMTPlayerState>();
	if (!MTPS || MTPS->IsReady())   // 준비 상태에선 변경 금지
	{
		return;
	}
	if (MTPS->GetSelectedCat() == NewCat)
	{
		return;   // 동일 선택은 무시 (불필요한 재스폰 방지)
	}
	MTPS->SetSelectedCat(NewCat);

	// 로비면 새 종류 폰으로 재스폰 (매치에선 GameMode가 null이라 자동 무시)
	if (AMTLobbyGameMode* Lobby = GetWorld() ? GetWorld()->GetAuthGameMode<AMTLobbyGameMode>() : nullptr)
	{
		Lobby->RespawnLobbyPawn(this);
	}
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

	// 전원 준비 시 자동 시작 판정
	if (AMTLobbyGameMode* Lobby = GetWorld() ? GetWorld()->GetAuthGameMode<AMTLobbyGameMode>() : nullptr)
	{
		Lobby->NotifyReadyChanged();
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
