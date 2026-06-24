#include "Game/MTLobbyGameMode.h"
#include "Player/MTPlayerState.h"
#include "Player/MTPlayerController.h"
#include "GameFramework/GameStateBase.h"

AMTLobbyGameMode::AMTLobbyGameMode()
{
	bUseSeamlessTravel = true;
	PlayerStateClass = AMTPlayerState::StaticClass();
	PlayerControllerClass = AMTPlayerController::StaticClass();
}

void AMTLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (UsedSlots.Num() != MaxPlayers)
	{
		UsedSlots.Init(false, MaxPlayers);
	}

	if (NewPlayer)
	{
		if (AMTPlayerState* MTPS = NewPlayer->GetPlayerState<AMTPlayerState>())
		{
			MTPS->SetPlayerSlot(AcquireSlot());
			MTPS->SetHost(NewPlayer->IsLocalController());   // 리슨 서버의 로컬 PC = 호스트
		}
	}
}

void AMTLobbyGameMode::Logout(AController* Exiting)
{
	if (Exiting)
	{
		if (AMTPlayerState* MTPS = Exiting->GetPlayerState<AMTPlayerState>())
		{
			ReleaseSlot(MTPS->GetPlayerSlot());
		}
	}
	Super::Logout(Exiting);
}

bool AMTLobbyGameMode::CanStartMatch() const
{
	const AGameStateBase* GS = GameState;
	if (!GS || GS->PlayerArray.Num() == 0)
	{
		return false;
	}
	for (const APlayerState* PS : GS->PlayerArray)
	{
		const AMTPlayerState* MTPS = Cast<AMTPlayerState>(PS);
		if (!MTPS || !MTPS->IsReady())
		{
			return false;
		}
	}
	return true;
}

void AMTLobbyGameMode::TryStartMatch()
{
	if (!HasAuthority() || !CanStartMatch())
	{
		return;
	}
	// 리슨 서버로 전환 (M2에서 세션 잠금·입력 차단·Trans_Map으로 확장)
	GetWorld()->ServerTravel(NextMapPath + TEXT("?listen"));
}

int32 AMTLobbyGameMode::AcquireSlot()
{
	for (int32 i = 0; i < UsedSlots.Num(); ++i)
	{
		if (!UsedSlots[i])
		{
			UsedSlots[i] = true;
			return i;
		}
	}
	return -1;   // 만석
}

void AMTLobbyGameMode::ReleaseSlot(int32 Slot)
{
	if (UsedSlots.IsValidIndex(Slot))
	{
		UsedSlots[Slot] = false;
	}
}
