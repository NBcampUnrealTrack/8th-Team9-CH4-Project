#include "Game/MTMatchGameMode.h"
#include "Game/MTGameInstance.h"
#include "Game/MTGameState.h"
#include "Game/MTLog.h"
#include "Online/MTSessionSubsystem.h"
#include "Online/MTOnlineUtils.h"
#include "Player/MTPlayerState.h"
#include "Player/MTPlayerController.h"
#include "Player/MTPlayerCharacter.h"
#include "UI/InGame/MTPlayerHUD.h"
#include "Pedestrian/MTPedestrianBase.h"
#include "Pedestrian/MTPedestrianSpawnManager.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "NavigationSystem.h"
#include "UObject/ConstructorHelpers.h"

AMTMatchGameMode::AMTMatchGameMode()
{
	bDelayedStart = true;        // 전원 로딩 전까지 매치 보류
	bUseSeamlessTravel = true;

	GameStateClass = AMTGameState::StaticClass();
	PlayerStateClass = AMTPlayerState::StaticClass();
	PlayerControllerClass = AMTPlayerController::StaticClass();
	HUDClass = AMTPlayerHUD::StaticClass();

	// 슬롯별 팀색 (빨·파·초·노)
	TeamColors = {
		FLinearColor(1.f, 0.2f, 0.2f),
		FLinearColor(0.2f, 0.4f, 1.f),
		FLinearColor(0.2f, 0.8f, 0.2f),
		FLinearColor(1.f, 0.8f, 0.f),
	};

	//PedestrianGenerationConfig = UMTPedestrianSpawnManager::MakeTestConfiguration();
}

void AMTMatchGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);

	// 이미 상위에서 거부됐으면 그 사유 유지
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}
	// 매치 맵에 도달했다는 것 자체가 게임 시작 → 신규 접속 거부.
	// (로비에서 함께 넘어온 플레이어는 seamless travel이라 이 경로를 안 탐)
	ErrorMessage = TEXT("게임이 이미 시작되어 참여할 수 없습니다.");
}

void AMTMatchGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    MarkLoaded(NewPlayer);
    AssignTeamColor(NewPlayer);     // 슬롯 기준 팀색 (직접 진입 시 폴백 포함)
    LockPreMatchInput(NewPlayer);   // 대기·카운트다운 동안 시야 고정
    // PIE 직행 등 로비를 안 거친 접속 — Null OSS면 "Player N" (seamless는 로비 이름 유지)
    UMTOnlineUtils::ApplyFallbackPlayerName(NewPlayer ? NewPlayer->PlayerState : nullptr);

    if (MTLogEnabled())
    {
        UE_LOG(LogMT, Log, TEXT("[MTMatch] PostLogin(신규접속) PC=%s Local=%d Auth=%d NetMode=%d"),
            *GetNameSafe(NewPlayer),
            (NewPlayer && NewPlayer->IsLocalController()) ? 1 : 0,
            HasAuthority() ? 1 : 0, (int32)GetNetMode());
    }
}

void AMTMatchGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
	Super::HandleSeamlessTravelPlayer(C);
	MarkLoaded(C);                  // seamless travel 완료 = 새 맵 로딩 완료
	AssignTeamColor(C);             // 로비에서 운반된 슬롯으로 팀색 결정
	LockPreMatchInput(C);           // 대기·카운트다운 동안 시야 고정

	if (MTLogEnabled())
	{
		UE_LOG(LogMT, Log, TEXT("[MTMatch] HandleSeamlessTravelPlayer C=%s Local=%d"),
			*GetNameSafe(C), (C && C->IsLocalController()) ? 1 : 0);
	}
}

void AMTMatchGameMode::HandleMatchHasEnded()
{
    Super::HandleMatchHasEnded();

    AMTGameState* GS = GetGameState<AMTGameState>();
    if (!GS) return;

    // 결과 화면 동안 행인 AI 정지 (서버에서 멈추면 이동 복제로 전 클라 정지)
    TArray<AActor*> Pedestrians;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AMTPedestrianBase::StaticClass(), Pedestrians);
    for (AActor* A : Pedestrians)
    {
        if (AMTPedestrianBase* Ped = Cast<AMTPedestrianBase>(A))
        {
            Ped->FreezeForMatchEnd();
        }
    }

    // 결과 화면 동안 고양이도 정지 (진행 중 스킬 취소 + 이동 잠금 — 행인과 동일 패턴)
    for (APlayerState* PS : GS->PlayerArray)
    {
        if (AMTPlayerCharacter* Cat = Cast<AMTPlayerCharacter>(PS->GetPawn()))
        {
            Cat->FreezeForMatchEnd();
        }
    }

    for (APlayerState* PS : GS->PlayerArray)
    {
        if (AMTPlayerController* PC = Cast<AMTPlayerController>(PS->GetPlayerController()))
        {
            PC->ClientShowResult();
        }
    }

	GS->Multicast_PlayMatchResultSFX();

    // 결과 화면 표시 후 자동 복귀 (버튼 미입력 대비 타임아웃)
    GetWorldTimerManager().SetTimer(LobbyTimer, this, &AMTMatchGameMode::ReturnToLobby, ResultScreenDuration, false);
}

void AMTMatchGameMode::ReturnToLobby()
{
	// 세션 유지한 채 전원 로비로 복귀 (리슨 서버: 호스트 권위 ServerTravel)
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}
	const UMTGameInstance* GI = Cast<UMTGameInstance>(GetGameInstance());
	const FString LobbyPath = GI ? GI->GetLobbyMapPath() : FString();
	if (!ensureMsgf(!LobbyPath.IsEmpty(), TEXT("로비 맵 미지정 — UMTGameInstance::LobbyMap 확인")))
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(LobbyTimer);   // 자동/버튼 중복 트래블 방지
	GetWorld()->ServerTravel(LobbyPath + TEXT("?listen"));
}

void AMTMatchGameMode::UpdateMatchTimer()
{
    RemainingTime--;

    // 남은 시간 복제 → 클라 HUD 표시
    if (AMTGameState* GS = GetGameState<AMTGameState>())
    {
        GS->SetMatchRemainingTime(RemainingTime);
    }

    if (RemainingTime <= 0)
    {
        GetWorldTimerManager().ClearTimer(MatchTimerHandle);
        EndMatch();
    }

}

void AMTMatchGameMode::SpawnInitialPedestrians()
{
	if (NormalPedestrianClasses.Num() == 0) return;

    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!NavSys) return;

    // 배치한 타깃 포인트 중 태그를 포함한 포인트 가져오기
    TArray<AActor*> StreetPoints;
    UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("MainStreet"), StreetPoints);

    if (StreetPoints.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MTMatch] 대로 스폰 포인트('MainStreet' 태그)가 맵에 없습니다!"));
        return;
    }

    for (int32 i = 0; i < MaxNormalPedestrians; ++i)
    {
        // 2. 배치된 대로 포인트 중 하나를 무작위 선택
        int32 RandomIdx = FMath::RandRange(0, StreetPoints.Num() - 1);
        FVector PivotLocation = StreetPoints[RandomIdx]->GetActorLocation();

        FNavLocation RandomNavLocation;

		// 대로 포인트 주변 설정 반경 안에서 내비 바닥 찾기
		bool bProjectSuccess = NavSys->GetRandomReachablePointInRadius(PivotLocation, PedestrianSpawnRadius, RandomNavLocation);

        FVector SpawnLocation = bProjectSuccess ? RandomNavLocation.Location : PivotLocation;
        SpawnLocation.Z += 50.f;

        FRotator SpawnRotation = FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f);

		// 종류 풀 순환 → 골고루 소환
		const TSubclassOf<AMTPedestrianBase> PedClass = NormalPedestrianClasses[i % NormalPedestrianClasses.Num()];
		if (!PedClass) continue;
		GetWorld()->SpawnActor<AActor>(PedClass, SpawnLocation, SpawnRotation);
    }
}

void AMTMatchGameMode::MarkLoaded(AController* C)
{
	if (C)
	{
		if (AMTPlayerState* MTPS = C->GetPlayerState<AMTPlayerState>())
		{
			MTPS->SetLoaded(true);
		}
	}
}

void AMTMatchGameMode::LockPreMatchInput(AController* C)
{
	// 대기·카운트다운 중엔 폰이 없어 컨트롤러가 직접 시야를 돌린다 → 서버가 소유 클라에 잠금 지시.
	// StartMatch의 폰 스폰(ClientRestart)에서 엔진이 잠금을 초기화하므로 별도 해제는 불필요.
	if (HasMatchStarted())
	{
		return;
	}
	if (APlayerController* PC = Cast<APlayerController>(C))
	{
		PC->ClientIgnoreLookInput(true);
		PC->ClientIgnoreMoveInput(true);
	}
}

void AMTMatchGameMode::AssignTeamColor(AController* C)
{
	AMTPlayerState* MTPS = C ? C->GetPlayerState<AMTPlayerState>() : nullptr;
	if (!MTPS)
	{
		return;
	}

	// 정식 배정은 로비(AMTLobbyGameMode)가 슬롯과 함께 수행 → CopyProperties로 운반됨.
	// 슬롯이 유효하면 색도 이미 실려 온 것 — 로비 미경유 직접 진입(PIE 등)만 진입 순서로 폴백.
	int32 Slot = MTPS->GetPlayerSlot();
	if (TeamColors.IsValidIndex(Slot))
	{
		return;
	}

	Slot = NextColorSlot++;
	MTPS->SetPlayerSlot(Slot);   // 폴백 슬롯도 기록 (결과/표시 일관성)
	if (TeamColors.IsValidIndex(Slot))
	{
		MTPS->SetTeamColor(TeamColors[Slot]);
	}
}

UClass* AMTMatchGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	// 로비에서 운반된 선택 고양이로 폰 클래스 결정 (없으면 DefaultPawnClass)
	if (InController)
	{
		if (const AMTPlayerState* MTPS = InController->GetPlayerState<AMTPlayerState>())
		{
			const EMTCatType Cat = MTPS->GetSelectedCat();
			if (const TSubclassOf<APawn>* Found = CatPawnClasses.Find(Cat))
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

AActor* AMTMatchGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// 플레이어 스타트
    AMTPlayerState* MTPS = Player->GetPlayerState<AMTPlayerState>();
    if (!MTPS) return Super::ChoosePlayerStart_Implementation(Player);

    int32 Slot = MTPS->GetPlayerSlot();
    FString TagName = FString::Printf(TEXT("Slot%d"), Slot);

    TArray<APlayerStart*> ValidStarts;
    for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
    {
        if (It->PlayerStartTag == FName(*TagName))
        {
            ValidStarts.Add(*It);
        }
    }

    if (ValidStarts.Num() == 0)
        return Super::ChoosePlayerStart_Implementation(Player);

    APlayerStart* Chosen = nullptr;

    if (RespawningControllers.Contains(Player))
    {
        // 리스폰: 전체 중 랜덤
        Chosen = ValidStarts[FMath::RandRange(0, ValidStarts.Num() - 1)];
    }
    else
    {
        // 최초 스폰: 미사용 포인트 우선
        TArray<APlayerStart*> UnusedStarts = ValidStarts.FilterByPredicate([this](APlayerStart* S)
        {
            return !UsedStarts.Contains(S);
        });

        if (UnusedStarts.Num() > 0)
        {
            Chosen = UnusedStarts[FMath::RandRange(0, UnusedStarts.Num() - 1)];
            UsedStarts.Add(Chosen);
        }
        else
        {
            Chosen = ValidStarts[FMath::RandRange(0, ValidStarts.Num() - 1)];
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[MT] 슬롯 %d → 스폰 포인트: %s"), Slot, *Chosen->GetName());

    return Chosen;
}

void AMTMatchGameMode::RequestRespawn(AController* Controller)
{
	// 리스폰
    if (!Controller) return;

    UE_LOG(LogTemp, Log, TEXT("[MT] 리스폰 대기 중: %s"), *Controller->GetName());

    TWeakObjectPtr<AController> WeakController = Controller;

    FTimerHandle RespawnTimer;
    GetWorldTimerManager().SetTimer(RespawnTimer, [this, WeakController]()
    {
        if (!WeakController.IsValid()) return;

        AController* C = WeakController.Get();
        RespawningControllers.Add(C);

        AActor* StartSpot = ChoosePlayerStart(C);
        RestartPlayerAtPlayerStart(C, StartSpot);

        RespawningControllers.Remove(C);

        UE_LOG(LogTemp, Log, TEXT("[MT] 리스폰 완료: %s"), *C->GetName());
    }, RespawnDelay, false);
}

bool AMTMatchGameMode::ReadyToStartMatch_Implementation()
{
	// 아직 이동 중인 플레이어가 있으면 대기 (조기 시작 방지)
	if (NumTravellingPlayers > 0)
	{
		return false;
	}

	const AGameStateBase* GS = GameState;
	if (!GS || GS->PlayerArray.Num() == 0)
	{
		return false;
	}

	for (const APlayerState* PS : GS->PlayerArray)
	{
		const AMTPlayerState* MTPS = Cast<AMTPlayerState>(PS);
		if (!MTPS || !MTPS->IsLoaded())
		{
			return false;
		}
	}

	// 전원 로딩 완료 → 시작 카운트다운(3·2·1) 경과 후 StartMatch
	if (bStartCountdownFinished)
	{
		return true;
	}
	if (!bStartCountdownRunning)
	{
		bStartCountdownRunning = true;

		MulticastPlayCountdownTickSound();

		if (StartCountdownSeconds <= 0)
		{
			bStartCountdownFinished = true;   // 카운트다운 미사용 → 즉시 시작
			return true;
		}
		if (AMTGameState* MTGS = GetGameState<AMTGameState>())
		{
			MTGS->SetMatchStartCountdown(StartCountdownSeconds);
		}
		// 첫 감소는 페이드인이 끝난 뒤부터 (첫 값 복제 = 클라 페이드 시작 신호)
		GetWorldTimerManager().SetTimer(StartCountdownTimer, this, &AMTMatchGameMode::TickStartCountdown,
			1.f, true, CountdownRevealDelay + 1.f);
	}
	return false;
}

void AMTMatchGameMode::TickStartCountdown()
{
	AMTGameState* MTGS = GetGameState<AMTGameState>();
	if (!MTGS)
	{
		return;
	}

	const int32 Next = MTGS->GetMatchStartCountdown() - 1;
	MTGS->SetMatchStartCountdown(Next);   // 0 = HUD "시작!" 표시

	if (Next <= 0)
	{
		GetWorldTimerManager().ClearTimer(StartCountdownTimer);
		bStartCountdownFinished = true;   // 다음 틱 ReadyToStartMatch → StartMatch
	}
}

void AMTMatchGameMode::MulticastPlayCountdownTickSound_Implementation()
{
	if (CountdownTickSound)
	{
		UGameplayStatics::PlaySound2D(this, CountdownTickSound);
	}
}

void AMTMatchGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();
	UE_LOG(LogTemp, Log, TEXT("[MTMatch] 전원 로딩 완료 → StartMatch (InProgress)"));

	// 게임 시작 → 세션을 검색·조인 불가로 (중도 참여 차단, 방 목록에서도 숨김)
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMTSessionSubsystem* Sessions = GI->GetSubsystem<UMTSessionSubsystem>())
		{
			Sessions->SetSessionJoinable(false);
		}
	}

	if (MTLogEnabled())
	{
		const AGameStateBase* GS = GameState;
		UE_LOG(LogMT, Log, TEXT("[MTMatch] HandleMatchHasStarted NetMode=%d Players=%d"),
			(int32)GetNetMode(), GS ? GS->PlayerArray.Num() : -1);
	}
	// TODO: 입력 권한 부여. 클라는 GameState->IsMatchInProgress()로 게이트.

	// 1. 매치 타이머 초기화 및 시작
    RemainingTime = MatchDuration;

    // 시작 시점 남은 시간을 클라이언트에 동기화
    if (AMTGameState* GS = GetGameState<AMTGameState>())
    {
        GS->SetMatchRemainingTime(RemainingTime);
    }

    GetWorldTimerManager().SetTimer(MatchTimerHandle, this, &AMTMatchGameMode::UpdateMatchTimer, 1.0f, true);

    // 2. 기획서: 행인 랜덤 스폰 루프 시작
    SpawnInitialPedestrians();
}
