#include "UI/Lobby/MTLobbyWidget.h"

#include "Player/MTPlayerController.h"
#include "Player/MTPlayerState.h"
#include "Game/MTGameInstance.h"
#include "Game/MTLog.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

void UMTLobbyWidget::LobbySetup()
{
	AddToViewport();
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
	ActivateWidget();   // CommonUI가 입력 모드/커서 관리

	// 로스터/타 플레이어 상태 변화를 주기적으로 UI에 반영 (로비는 저빈도라 충분)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RefreshTimer, this, &UMTLobbyWidget::HandleRefresh, 0.4f, true);
	}
	HandleRefresh();
}

void UMTLobbyWidget::SelectCat(EMTCatType CatType)
{
	if (AMTPlayerController* PC = GetMTPC())
	{
		PC->Server_SetSelectedCat(CatType);
	}
}

void UMTLobbyWidget::ToggleReady()
{
	if (AMTPlayerController* PC = GetMTPC())
	{
		PC->Server_ToggleReady();
	}
}

void UMTLobbyWidget::RequestStart()
{
	if (AMTPlayerController* PC = GetMTPC())
	{
		PC->Server_RequestStartMatch();
	}
}

void UMTLobbyWidget::LeaveLobby()
{
	// 나가기 = Flow 책임
	if (UMTGameInstance* GI = Cast<UMTGameInstance>(GetGameInstance()))
	{
		GI->LeaveGame();
	}
}

TArray<AMTPlayerState*> UMTLobbyWidget::GetPlayers() const
{
	TArray<AMTPlayerState*> Result;
	if (const UWorld* World = GetWorld())
	{
		if (const AGameStateBase* GS = World->GetGameState())
		{
			for (APlayerState* PS : GS->PlayerArray)
			{
				if (AMTPlayerState* MTPS = Cast<AMTPlayerState>(PS))
				{
					Result.Add(MTPS);
				}
			}
		}
	}
	return Result;
}

AMTPlayerState* UMTLobbyWidget::GetLocalPlayerState() const
{
	return GetLocalMTPS();
}

AMTPlayerState* UMTLobbyWidget::GetPlayerInSlot(int32 SlotIndex) const
{
	for (AMTPlayerState* MTPS : GetPlayers())
	{
		if (MTPS && MTPS->GetPlayerSlot() == SlotIndex)
		{
			return MTPS;   // 해당 슬롯 점유자
		}
	}
	return nullptr;        // 빈 슬롯 → BP에서 "대기 중"
}

bool UMTLobbyWidget::IsLocalReady() const
{
	const AMTPlayerState* MTPS = GetLocalMTPS();
	return MTPS && MTPS->IsReady();
}

EMTCatType UMTLobbyWidget::GetLocalSelectedCat() const
{
	const AMTPlayerState* MTPS = GetLocalMTPS();
	return MTPS ? MTPS->GetSelectedCat() : EMTCatType::None;
}

bool UMTLobbyWidget::IsHost() const
{
	const APlayerController* PC = GetOwningPlayer();
	return PC && PC->HasAuthority();
}

bool UMTLobbyWidget::CanStart() const
{
	const TArray<AMTPlayerState*> Players = GetPlayers();
	if (Players.Num() < 2)   // 호스트 혼자 시작 불가 (최소 2명)
	{
		return false;
	}
	for (const AMTPlayerState* MTPS : Players)
	{
		if (!MTPS)
		{
			return false;
		}
		if (MTPS->IsHost())
		{
			continue;   // 호스트는 준비 불필요
		}
		if (!MTPS->IsReady())
		{
			return false;
		}
	}
	return true;
}

void UMTLobbyWidget::HandleRefresh()
{
	// 진단: PlayerArray가 비었는지/슬롯이 배정됐는지 (MT.Log 1일 때만)
	if (MTLogEnabled())
	{
		const TArray<AMTPlayerState*> Players = GetPlayers();
		const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
		const int32 RawCount = GS ? GS->PlayerArray.Num() : -1;

		FString Info = FString::Printf(TEXT("[MTLobby] MTPlayers=%d, RawPS=%d | "), Players.Num(), RawCount);
		for (const AMTPlayerState* MTPS : Players)
		{
			if (MTPS)
			{
				Info += FString::Printf(TEXT("[slot=%d host=%d] "), MTPS->GetPlayerSlot(), MTPS->IsHost() ? 1 : 0);
			}
		}
		UE_LOG(LogMT, Log, TEXT("%s"), *Info);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(1001, 1.f, FColor::Yellow, Info);   // 같은 Key=한 줄 갱신
		}
	}

	OnLobbyRefresh.Broadcast();
}

void UMTLobbyWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}
	Super::NativeDestruct();
}

TOptional<FUIInputConfig> UMTLobbyWidget::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

AMTPlayerController* UMTLobbyWidget::GetMTPC() const
{
	return Cast<AMTPlayerController>(GetOwningPlayer());
}

AMTPlayerState* UMTLobbyWidget::GetLocalMTPS() const
{
	const APlayerController* PC = GetOwningPlayer();
	return PC ? PC->GetPlayerState<AMTPlayerState>() : nullptr;
}
