#include "UI/Lobby/MTLobbyWidget.h"

#include "Player/MTPlayerController.h"
#include "Player/MTPlayerState.h"
#include "Game/MTGameInstance.h"
#include "Game/MTLog.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
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

	// 로비 월드가 정리되면 위젯도 함께 제거 (호스트 하드 LoadMap 포함 → 좀비 위젯 방지)
	if (!WorldCleanupHandle.IsValid())
	{
		WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &UMTLobbyWidget::HandleWorldCleanup);
		if (MTLogEnabled())
		{
			const APlayerController* PC = GetOwningPlayer();
			const FString Msg = FString::Printf(TEXT("[MTLobby] LobbySetup: Cleanup 바인딩 | Host(Auth)=%d"),
				(PC && PC->HasAuthority()) ? 1 : 0);
			UE_LOG(LogMT, Log, TEXT("%s"), *Msg);
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Cyan, Msg);
		}
	}
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

bool UMTLobbyWidget::IsLocalPlayerSlot(int32 SlotIndex) const
{
	const AMTPlayerState* Local = GetLocalMTPS();
	return Local && Local->GetPlayerSlot() == SlotIndex;
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
				// 복제 진단: 다른 클라가 준비하면 ready 값이 모든 화면에서 바뀌어야 정상
					Info += FString::Printf(TEXT("[slot=%d host=%d ready=%d] "),
						MTPS->GetPlayerSlot(), MTPS->IsHost() ? 1 : 0, MTPS->IsReady() ? 1 : 0);
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

void UMTLobbyWidget::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	const bool bMine = (World == GetWorld());
	if (MTLogEnabled())
	{
		const FString Msg = FString::Printf(TEXT("[MTLobby] WorldCleanup 수신 bMine=%d | World=%s MyWorld=%s"),
			bMine ? 1 : 0, *GetNameSafe(World), *GetNameSafe(GetWorld()));
		UE_LOG(LogMT, Log, TEXT("%s"), *Msg);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Cyan, Msg);
	}
	if (bMine)
	{
		DeactivateWidget();      // CommonUI 활성 트리에서 빠져 Menu 입력모드 해제
		RemoveFromParent();
	}
}

void UMTLobbyWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	WorldCleanupHandle.Reset();
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
