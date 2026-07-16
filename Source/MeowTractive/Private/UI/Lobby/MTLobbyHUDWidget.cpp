#include "UI/Lobby/MTLobbyHUDWidget.h"
#include "Game/MTLobbyGameState.h"
#include "Player/MTPlayerState.h"
#include "Player/MTPlayerController.h"
#include "Online/MTOnlineUtils.h"
#include "Game/MTGameInstance.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"

void UMTLobbyHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 방정보/카운트다운은 이벤트 구독으로 즉시 갱신
	if (AMTLobbyGameState* GS = GetLobbyGS())
	{
		GS->OnRoomSettingsChanged.AddDynamic(this, &UMTLobbyHUDWidget::HandleRoomSettingsChanged);
		GS->OnStartCountdownChanged.AddDynamic(this, &UMTLobbyHUDWidget::HandleCountdownChanged);
		HandleCountdownChanged(GS->GetStartCountdown());   // 초기값
	}

	// 로스터/준비 변화는 폴백 (저빈도라 충분)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RefreshTimer, this, &UMTLobbyHUDWidget::PollRefresh, 0.4f, true);
	}
	OnRefresh.Broadcast();
}

void UMTLobbyHUDWidget::NativeDestruct()
{
	if (AMTLobbyGameState* GS = GetLobbyGS())
	{
		GS->OnRoomSettingsChanged.RemoveDynamic(this, &UMTLobbyHUDWidget::HandleRoomSettingsChanged);
		GS->OnStartCountdownChanged.RemoveDynamic(this, &UMTLobbyHUDWidget::HandleCountdownChanged);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}
	Super::NativeDestruct();
}

TArray<AMTPlayerState*> UMTLobbyHUDWidget::GetPlayers() const
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

AMTPlayerState* UMTLobbyHUDWidget::GetLocalPlayerState() const
{
	const APlayerController* PC = GetOwningPlayer();
	return PC ? PC->GetPlayerState<AMTPlayerState>() : nullptr;
}

FString UMTLobbyHUDWidget::GetRoomName() const
{
	const AMTLobbyGameState* GS = GetLobbyGS();
	return GS ? GS->GetRoomName() : FString();
}

EMTRoomGameMode UMTLobbyHUDWidget::GetRoomGameMode() const
{
	const AMTLobbyGameState* GS = GetLobbyGS();
	return GS ? GS->GetRoomGameMode() : EMTRoomGameMode::FFA;
}

EMTRoomMap UMTLobbyHUDWidget::GetRoomMap() const
{
	const AMTLobbyGameState* GS = GetLobbyGS();
	return GS ? GS->GetRoomMap() : EMTRoomMap::Park;
}

FText UMTLobbyHUDWidget::GetRoomSummaryText() const
{
	const AMTLobbyGameState* GS = GetLobbyGS();
	if (!GS)
	{
		return FText::GetEmpty();
	}

	const TCHAR* ModeStr =
		GS->GetRoomGameMode() == EMTRoomGameMode::Team ? TEXT("팀전") :
		GS->GetRoomGameMode() == EMTRoomGameMode::CatFight ? TEXT("고양이 전투") : TEXT("개인전");
	const TCHAR* MapStr =
		GS->GetRoomMap() == EMTRoomMap::Random ? TEXT("랜덤") : TEXT("공원");

	return FText::FromString(FString::Printf(TEXT("%s  |  %s  |  %s"),
		*GS->GetRoomName(), ModeStr, MapStr));
}

bool UMTLobbyHUDWidget::IsLocalReady() const
{
	const AMTPlayerState* Local = GetLocalPlayerState();
	return Local && Local->IsReady();
}

bool UMTLobbyHUDWidget::IsLocalHost() const
{
	const AMTPlayerState* Local = GetLocalPlayerState();
	return Local && Local->IsHost();
}

bool UMTLobbyHUDWidget::CanKick(AMTPlayerState* Target) const
{
	return IsLocalHost() && Target && Target != GetLocalPlayerState();
}

FText UMTLobbyHUDWidget::GetReadyButtonText() const
{
	return IsLocalReady()
		? FText::FromString(TEXT("준비 완료"))
		: FText::FromString(TEXT("게임 준비"));
}

void UMTLobbyHUDWidget::ToggleReady()
{
	if (AMTPlayerController* PC = GetMTPC())
	{
		PC->Server_ToggleReady();
	}
}

void UMTLobbyHUDWidget::KickPlayer(AMTPlayerState* Target)
{
	if (AMTPlayerController* PC = GetMTPC())
	{
		if (CanKick(Target))
		{
			PC->Server_KickPlayer(Target);
		}
	}
}

void UMTLobbyHUDWidget::LeaveLobby()
{
	// 나가기 = Flow(GameInstance)가 세션 정리 + 메뉴 복귀
	if (UMTGameInstance* GI = Cast<UMTGameInstance>(GetGameInstance()))
	{
		GI->LeaveGame();
	}
}

UTexture2D* UMTLobbyHUDWidget::GetAvatar(AMTPlayerState* Target)
{
	if (!Target)
	{
		return nullptr;
	}
	// 이미 받아둔 게 있으면 재사용 (매 리프레시마다 새 텍스처 생성 방지)
	if (const TObjectPtr<UTexture2D>* Cached = AvatarCache.Find(Target))
	{
		if (*Cached)
		{
			return *Cached;
		}
	}
	// 스팀에서 시도 — 성공 시에만 캐시 (아직 로딩 전이면 nullptr → 다음 호출에서 재시도)
	if (UTexture2D* Fetched = UMTOnlineUtils::GetSteamAvatar(Target))
	{
		AvatarCache.Add(Target, Fetched);
		return Fetched;
	}
	return nullptr;
}

AMTLobbyGameState* UMTLobbyHUDWidget::GetLobbyGS() const
{
	return GetWorld() ? GetWorld()->GetGameState<AMTLobbyGameState>() : nullptr;
}

AMTPlayerController* UMTLobbyHUDWidget::GetMTPC() const
{
	return Cast<AMTPlayerController>(GetOwningPlayer());
}

void UMTLobbyHUDWidget::HandleRoomSettingsChanged()
{
	OnRefresh.Broadcast();
}

void UMTLobbyHUDWidget::HandleCountdownChanged(int32 RemainingSeconds)
{
	OnCountdown.Broadcast(RemainingSeconds);
}

void UMTLobbyHUDWidget::PollRefresh()
{
	OnRefresh.Broadcast();
}
