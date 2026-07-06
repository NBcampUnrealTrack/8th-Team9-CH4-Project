#include "Game/MTLobbyGameMode.h"
#include "Game/MTLobbyGameState.h"
#include "Game/MTGameInstance.h"
#include "Player/MTPlayerState.h"
#include "Player/MTPlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/GameSession.h"

AMTLobbyGameMode::AMTLobbyGameMode()
{
	bUseSeamlessTravel = true;
	GameStateClass = AMTLobbyGameState::StaticClass();
	PlayerStateClass = AMTPlayerState::StaticClass();
	PlayerControllerClass = AMTPlayerController::StaticClass();

	MatchMapPaths.Add(EMTRoomMap::Insadong, TEXT("/Game/Map/Map_Insa/Prototype_Insadong"));
}

void AMTLobbyGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 방 생성 시 GameInstance에 저장된 설정을 로비 GameState로 (전 클라 복제)
	if (AMTLobbyGameState* GS = GetGameState<AMTLobbyGameState>())
	{
		if (const UMTGameInstance* GI = Cast<UMTGameInstance>(GetGameInstance()))
		{
			GS->SetRoomSettings(GI->GetRoomSettings());
		}
	}
}

void AMTLobbyGameMode::ApplyRoomSettings(AController* Requestor, FMTRoomSettings NewSettings)
{
	// 호스트 검증: 리슨 서버에서 로컬 컨트롤러 = 방장
	if (!Requestor || !Requestor->IsLocalController())
	{
		return;
	}

	AMTLobbyGameState* GS = GetGameState<AMTLobbyGameState>();
	if (!GS)
	{
		return;
	}

	// 이름 비우면 기존 이름 유지
	if (NewSettings.RoomName.TrimStartAndEnd().IsEmpty())
	{
		NewSettings.RoomName = GS->GetRoomName();
	}

	GS->SetRoomSettings(NewSettings);   // 전 클라 복제 (플레이어는 그대로)

	// 세션 광고 갱신 (검색 목록에 새 이름/설정 반영)
	if (UMTGameInstance* GI = Cast<UMTGameInstance>(GetGameInstance()))
	{
		GI->ApplyRoomSettings(NewSettings);
	}
}

void AMTLobbyGameMode::KickPlayer(AController* Requestor, APlayerState* Target)
{
	if (!Requestor || !Requestor->IsLocalController() || !Target)
	{
		return;   // 호스트만 강퇴 가능
	}

	APlayerController* TargetPC = Target->GetPlayerController();
	if (!TargetPC || TargetPC->IsLocalController())
	{
		return;   // 호스트 자신은 강퇴 불가
	}

	if (GameSession)
	{
		GameSession->KickPlayer(TargetPC, FText::FromString(TEXT("방장에 의해 강퇴되었습니다.")));
	}
	// 슬롯 반환은 Logout에서 처리됨
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

FString AMTLobbyGameMode::ResolveMatchMapPath() const
{
	const AMTLobbyGameState* GS = GetGameState<AMTLobbyGameState>();
	const EMTRoomMap Selected = GS ? GS->GetRoomMap() : EMTRoomMap::Insadong;

	if (Selected == EMTRoomMap::Random)
	{
		TArray<FString> Candidates;
		MatchMapPaths.GenerateValueArray(Candidates);
		if (Candidates.Num() > 0)
		{
			return Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
		}
	}
	else if (const FString* Found = MatchMapPaths.Find(Selected))
	{
		return *Found;
	}
	return NextMapPath;   // 폴백
}

void AMTLobbyGameMode::TryStartMatch()
{
	if (!HasAuthority() || !CanStartMatch())
	{
		return;
	}
	// 방 설정의 맵으로 리슨 서버 전환
	GetWorld()->ServerTravel(ResolveMatchMapPath() + TEXT("?listen"));
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
