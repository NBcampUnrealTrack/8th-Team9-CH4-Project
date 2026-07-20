#include "Player/MTPlayerController.h"
#include "Player/MTPlayerState.h"
#include "Player/MTPlayerCharacter.h"
#include "UI/InGame/MTMatchGameResultWidget.h"
#include "UI/PauseMenu/MTPauseMenuWidget.h"
#include "Game/MTLobbyGameMode.h"
#include "Game/MTMatchGameMode.h"
#include "Game/MTGameInstance.h"
#include "Game/MTLog.h"
#include "EnhancedInputComponent.h"
#include "InputCoreTypes.h"
#include "UI/InGame/MTPlayerHUD.h"
#include "UI/InGame/MTPlayerWidget.h"

void AMTPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// IA_Pause(ESC, PIE용 P) → 일시정지 메뉴 토글. 컨텍스트(IMC_Default)는 캐릭터가 등록.
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (PauseAction)
		{
			EIC->BindAction(PauseAction, ETriggerEvent::Started, this, &AMTPlayerController::TogglePauseMenu);
		}
		else
		{
			UE_CLOG(MTLogEnabled(), LogMT, Warning, TEXT("[MTPC] PauseAction 미지정 → 일시정지 입력 없음 (BP_MTPlayerController에서 IA_Pause 지정)"));
		}
	}

	// F1 → 스킬 정보 패널 토글 (Enhanced Input과 별개로 컨트롤러 InputComponent에 직접 바인딩)
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::F1, IE_Pressed, this, &AMTPlayerController::ToggleSkillInfo);
	}
}

void AMTPlayerController::ToggleSkillInfo()
{
	if (!IsLocalController())
	{
		return;
	}
	if (AMTPlayerHUD* MTHUD = GetHUD<AMTPlayerHUD>())
	{
		if (UMTPlayerWidget* Widget = MTHUD->GetPlayerWidget())
		{
			Widget->ToggleSkillInfo();
		}
	}
}

void AMTPlayerController::ClientShowHitMarker_Implementation()
{
	if (AMTPlayerHUD* MTHUD = GetHUD<AMTPlayerHUD>())
	{
		if (UMTPlayerWidget* Widget = MTHUD->GetPlayerWidget())
		{
			Widget->ShowHitMarker();
		}
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

	// UIOnly 전환 시 눌린 키 release가 합성돼 홀드형 스킬이 발사되는 것 방지 — 시전 중 스킬 취소
	if (AMTPlayerCharacter* Cat = Cast<AMTPlayerCharacter>(GetPawn()))
	{
		Cat->CancelActiveSkills();
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

	// 결과 화면 표시 — 중복 호출돼도 1개만 유지 (행 누적/이중 블러 방지)
	if (ResultWidgetClass && !ResultWidget)
    {
        ResultWidget = CreateWidget<UMTMatchGameResultWidget>(this, ResultWidgetClass);
        if (ResultWidget)
        {
            ResultWidget->AddToViewport();
            ResultWidget->ShowResult();
        }
    }
}
