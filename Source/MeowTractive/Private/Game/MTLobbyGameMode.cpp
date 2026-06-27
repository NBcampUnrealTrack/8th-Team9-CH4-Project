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
	SetupLobbyPlayer(NewPlayer);   // 신규 접속(세션 참가)
}

void AMTLobbyGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
	Super::HandleSeamlessTravelPlayer(C);
	SetupLobbyPlayer(C);           // 매치→로비 복귀: 슬롯 그대로 재사용
}

void AMTLobbyGameMode::SetupLobbyPlayer(AController* C)
{
	if (UsedSlots.Num() != MaxPlayers)
	{
		UsedSlots.Init(false, MaxPlayers);
	}

	AMTPlayerState* MTPS = C ? C->GetPlayerState<AMTPlayerState>() : nullptr;
	if (!MTPS)
	{
		return;
	}

	// 매치에서 돌아온 경우 PlayerState에 슬롯이 실려 옴 → 그 슬롯 그대로 점유. 없으면 새로 배정.
	const int32 Carried = MTPS->GetPlayerSlot();
	if (UsedSlots.IsValidIndex(Carried) && !UsedSlots[Carried])
	{
		UsedSlots[Carried] = true;
	}
	else
	{
		MTPS->SetPlayerSlot(AcquireSlot());
	}

	MTPS->SetHost(C->IsLocalController());   // 리슨 서버의 로컬 PC = 호스트
	// 팀색은 매치 게임모드가 슬롯 기준으로 결정 (AMTMatchGameMode::AssignTeamColor)
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
	if (!GS || GS->PlayerArray.Num() < 2)   // 호스트 혼자 시작 불가 (최소 2명)
	{
		return false;
	}
	for (const APlayerState* PS : GS->PlayerArray)
	{
		const AMTPlayerState* MTPS = Cast<AMTPlayerState>(PS);
		if (!MTPS)
		{
			return false;
		}
		if (MTPS->IsHost())
		{
			continue;   // 호스트는 준비 불필요 (시작 권한 보유)
		}
		if (!MTPS->IsReady())
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
