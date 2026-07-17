#include "Game/MTLobbyGameMode.h"
#include "Game/MTLobbyGameState.h"
#include "Game/MTGameInstance.h"
#include "Online/MTSessionSubsystem.h"
#include "Online/MTOnlineUtils.h"
#include "Player/MTPlayerState.h"
#include "Player/MTPlayerController.h"
#include "Player/MTLobbyCharacter.h"
#include "UI/Lobby/MTLobbyHUD.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"

AMTLobbyGameMode::AMTLobbyGameMode()
{
	bUseSeamlessTravel = true;
	GameStateClass = AMTLobbyGameState::StaticClass();
	PlayerStateClass = AMTPlayerState::StaticClass();
	PlayerControllerClass = AMTPlayerController::StaticClass();
	HUDClass = AMTLobbyHUD::StaticClass();   // 도착 직후 검은 화면 → 페이드인 (BP HUD_Lobby가 오버라이드)

	// 네이티브 CDO 기본값은 쿠커가 소프트 참조로 추적 못 함 — BP_LobbyMode에서 덮어쓰고 MapsToCook 유지
	// TODO: 공원 맵 완성 시 실제 맵으로 교체 (현재는 프로토타입 맵 사용)
	FallbackMatchMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Map/TestMap.TestMap")));
	MatchMaps.Add(EMTRoomMap::Park, FallbackMatchMap);

	// 슬롯별 팀색 (빨·파·초·노) — AMTMatchGameMode 폴백 팔레트와 동일하게 유지
	TeamColors = {
		FLinearColor(1.f, 0.2f, 0.2f),
		FLinearColor(0.2f, 0.4f, 1.f),
		FLinearColor(0.2f, 0.8f, 0.2f),
		FLinearColor(1.f, 0.8f, 0.f),
	};
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

	// 로비(최초/매치 복귀)에선 다시 참여 가능하게 개방 (매치 시작 시 닫힌 것 복구)
	if (UMTSessionSubsystem* Sessions = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMTSessionSubsystem>() : nullptr)
	{
		Sessions->SetSessionJoinable(true);
	}

}

void AMTLobbyGameMode::RespawnLobbyPawn(AController* C)
{
	if (!HasAuthority() || !C)
	{
		return;
	}
	// 기존 폰 위치·카메라 방향을 캡처 → 그 자리에 새 고양이 스폰 (선택 시 제자리 교체)
	FTransform SpawnXform;
	bool bHasXform = false;
	const FRotator SavedViewRotation = C->GetControlRotation();
	if (APawn* Old = C->GetPawn())
	{
		SpawnXform = Old->GetActorTransform();
		bHasXform = true;
		C->UnPossess();
		Old->Destroy();
	}

	if (bHasXform)
	{
		RestartPlayerAtTransform(C, SpawnXform);   // 기존 위치에 GetDefaultPawnClassForController 폰

		// FinishRestartPlayer가 스폰 방향으로 리셋한 카메라를 교체 전 방향으로 복원
		C->SetControlRotation(SavedViewRotation);
		if (APlayerController* PC = Cast<APlayerController>(C))
		{
			PC->ClientSetRotation(SavedViewRotation);
		}
	}
	else
	{
		RestartPlayer(C);   // 폰이 없던 경우만 PlayerStart 폴백
	}
}

UClass* AMTLobbyGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	// 로비에서 선택한 고양이를 그대로 조종 (인게임과 동일 폰) → 컨트롤 연습
	if (InController)
	{
		if (const AMTPlayerState* MTPS = InController->GetPlayerState<AMTPlayerState>())
		{
			if (const TSubclassOf<APawn>* Found = CatPawnClasses.Find(MTPS->GetSelectedCat()))
			{
				if (*Found)
				{
					return *Found;
				}
			}
		}
	}
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

AActor* AMTLobbyGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// 슬롯별 PlayerStart("Slot%d") — 자기 조형물(OwnerSlot 동일) 앞에 스폰
	if (const AMTPlayerState* MTPS = Player ? Player->GetPlayerState<AMTPlayerState>() : nullptr)
	{
		const FString Tag = FString::Printf(TEXT("Slot%d"), MTPS->GetPlayerSlot());
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			if (It->PlayerStartTag == FName(*Tag))
			{
				return *It;
			}
		}
	}
	return Super::ChoosePlayerStart_Implementation(Player);
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

void AMTLobbyGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// 폰 스폰(Super)보다 먼저 슬롯·고양이 확정 — 스폰 위치와 폰 클래스가 이 값들로 결정된다
	SetupLobbyPlayer(NewPlayer);
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
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

	// 팀색은 로비에서 슬롯 기준으로 확정 → CopyProperties로 매치까지 운반
	const int32 Slot = MTPS->GetPlayerSlot();
	if (TeamColors.IsValidIndex(Slot))
	{
		MTPS->SetTeamColor(TeamColors[Slot]);
	}

	MTPS->SetHost(C->IsLocalController());   // 리슨 서버의 로컬 PC = 호스트
	UMTOnlineUtils::ApplyFallbackPlayerName(MTPS);   // Null OSS면 "Player N" (슬롯 배정 후)
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

	// 이탈로 인원/준비 조건이 깨지면 카운트다운 취소
	NotifyReadyChanged();
}

bool AMTLobbyGameMode::CanStartMatch() const
{
	const AGameStateBase* GS = GameState;
	if (!GS || GS->PlayerArray.Num() < 2)   // 최소 2명
	{
		return false;
	}
	// 3D 로비: 전원(호스트 포함) R키로 Ready → 자동 시작
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

void AMTLobbyGameMode::NotifyReadyChanged()
{
	if (!HasAuthority())
	{
		return;
	}

	AMTLobbyGameState* GS = GetGameState<AMTLobbyGameState>();
	if (!GS)
	{
		return;
	}

	if (CanStartMatch())
	{
		// 전원 준비 → 카운트다운 시작 (이미 돌고 있으면 유지)
		if (!GetWorldTimerManager().IsTimerActive(CountdownTimer))
		{
			GS->SetStartCountdown(AutoStartSeconds);
			GetWorldTimerManager().SetTimer(CountdownTimer, this, &AMTLobbyGameMode::TickCountdown, 1.f, true);
		}
	}
	else
	{
		// 준비 해제/이탈 → 취소
		GetWorldTimerManager().ClearTimer(CountdownTimer);
		GS->SetStartCountdown(-1);
	}
}

void AMTLobbyGameMode::TickCountdown()
{
	AMTLobbyGameState* GS = GetGameState<AMTLobbyGameState>();
	if (!GS || !CanStartMatch())   // 카운트다운 중 이탈/해제 대비 재검증
	{
		GetWorldTimerManager().ClearTimer(CountdownTimer);
		if (GS) { GS->SetStartCountdown(-1); }
		return;
	}

	const int32 Remaining = GS->GetStartCountdown() - 1;
	if (Remaining <= 0)
	{
		GetWorldTimerManager().ClearTimer(CountdownTimer);
		GS->SetStartCountdown(0);   // "시작!" 표시용
		TryStartMatch();
	}
	else
	{
		GS->SetStartCountdown(Remaining);
	}
}

FString AMTLobbyGameMode::ResolveMatchMapPath() const
{
	const AMTLobbyGameState* GS = GetGameState<AMTLobbyGameState>();
	const EMTRoomMap Selected = GS ? GS->GetRoomMap() : EMTRoomMap::Park;

	TSoftObjectPtr<UWorld> Map;
	if (Selected == EMTRoomMap::Random)
	{
		TArray<TSoftObjectPtr<UWorld>> Candidates;
		MatchMaps.GenerateValueArray(Candidates);
		if (Candidates.Num() > 0)
		{
			Map = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
		}
	}
	else if (const TSoftObjectPtr<UWorld>* Found = MatchMaps.Find(Selected))
	{
		Map = *Found;
	}

	if (Map.IsNull())
	{
		Map = FallbackMatchMap;   // 폴백
	}
	// ServerTravel은 패키지 경로 문자열을 받는다
	return Map.ToSoftObjectPath().GetLongPackageName();
}

void AMTLobbyGameMode::TryStartMatch()
{
	if (!HasAuthority() || !CanStartMatch())
	{
		return;
	}
	ForceStartMatch();
}

void AMTLobbyGameMode::ForceStartMatch()
{
	if (!HasAuthority())
	{
		return;
	}
	const FString MapPath = ResolveMatchMapPath();
	if (!ensureMsgf(!MapPath.IsEmpty(), TEXT("매치 맵 미지정 — MatchMaps/FallbackMatchMap 확인")))
	{
		return;
	}
	// 방 설정의 맵으로 리슨 서버 전환
	GetWorld()->ServerTravel(MapPath + TEXT("?listen"));
}

// 콘솔: `MT.Start 1` — 개발용 강제 시작. 인원/준비 조건 무시 (로비의 리슨 호스트 전용)
static FAutoConsoleCommandWithWorldAndArgs GMTStartCmd(
	TEXT("MT.Start"),
	TEXT("개발용: 로비에서 인원/준비 조건을 무시하고 즉시 매치 시작 (호스트 전용)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World)
		{
			AMTLobbyGameMode* Lobby = World ? World->GetAuthGameMode<AMTLobbyGameMode>() : nullptr;
			if (!Lobby)
			{
				UE_LOG(LogTemp, Warning, TEXT("[MT.Start] 로비의 호스트에서만 사용 가능합니다."));
				return;
			}
			UE_LOG(LogTemp, Log, TEXT("[MT.Start] 개발용 강제 시작"));
			Lobby->ForceStartMatch();
		}));

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
