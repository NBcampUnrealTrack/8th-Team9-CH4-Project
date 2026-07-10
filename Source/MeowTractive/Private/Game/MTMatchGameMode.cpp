#include "Game/MTMatchGameMode.h"
#include "Game/MTGameState.h"
#include "Game/MTLog.h"
#include "Player/MTPlayerState.h"
#include "Player/MTPlayerController.h"
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

	// 고양이 캐릭터로 교체
	static ConstructorHelpers::FClassFinder<APawn> PawnBP(TEXT("/Game/Blueprints/Player/BP_Cat"));
	if (PawnBP.Succeeded())
	{
		DefaultPawnClass = PawnBP.Class;
	}

	// 슬롯별 팀색 (빨·파·초·노)
	TeamColors = {
		FLinearColor(1.f, 0.2f, 0.2f),
		FLinearColor(0.2f, 0.4f, 1.f),
		FLinearColor(0.2f, 0.8f, 0.2f),
		FLinearColor(1.f, 0.8f, 0.f),
	};

	PedestrianGenerationConfig = UMTPedestrianSpawnManager::MakeTestConfiguration();
}

void AMTMatchGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    MarkLoaded(NewPlayer);
    AssignTeamColor(NewPlayer);     // 슬롯 기준 팀색 (직접 진입 시 폴백 포함)

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

    for (APlayerState* PS : GS->PlayerArray)
    {
        if (AMTPlayerController* PC = Cast<AMTPlayerController>(PS->GetPlayerController()))
        {
            PC->ClientShowResult();
        }
    }

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
	GetWorldTimerManager().ClearTimer(LobbyTimer);   // 자동/버튼 중복 트래블 방지
	GetWorld()->ServerTravel(LobbyMapPath + TEXT("?listen"));
}

void AMTMatchGameMode::UpdateMatchTimer()
{
    RemainingTime--;

    // 화면 가림 방지로 임시 주석 (남은 시간 디버그 표시)
    GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::White,
        FString::Printf(TEXT("남은 시간: %d"), RemainingTime));

    if (RemainingTime <= 0)
    {
        GetWorldTimerManager().ClearTimer(MatchTimerHandle);
        EndMatch();
    }

}

void AMTMatchGameMode::SpawnInitialPedestrians()
{
	if (!NormalPedestrianClass) return;

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
        
        // 대로 포인트 주변 15~20미터(1500.f~2000.f) 반경 안에서 내비 바닥 찾기
        bool bProjectSuccess = NavSys->GetRandomReachablePointInRadius(PivotLocation, 2000.f, RandomNavLocation);

        FVector SpawnLocation = bProjectSuccess ? RandomNavLocation.Location : PivotLocation;
        SpawnLocation.Z += 50.f;

        FRotator SpawnRotation = FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f);
		const FTransform SpawnTransform(SpawnRotation, SpawnLocation);
		UMTPedestrianSpawnManager::SpawnPedestrian(
			this,
			NormalPedestrianClass,
			SpawnTransform,
			PedestrianGenerationConfig);
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

void AMTMatchGameMode::AssignTeamColor(AController* C)
{
	AMTPlayerState* MTPS = C ? C->GetPlayerState<AMTPlayerState>() : nullptr;
	if (!MTPS)
	{
		return;
	}

	// 로비를 거치면 슬롯이 운반됨. 매치맵 직접 진입(PIE 등)이면 슬롯이 없으니 진입 순서로 폴백.
	int32 Slot = MTPS->GetPlayerSlot();
	if (!TeamColors.IsValidIndex(Slot))
	{
		Slot = NextColorSlot++;
		MTPS->SetPlayerSlot(Slot);   // 폴백 슬롯도 기록 (결과/표시 일관성)
	}

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
	return true;
}

void AMTMatchGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();
	UE_LOG(LogTemp, Log, TEXT("[MTMatch] 전원 로딩 완료 → StartMatch (InProgress)"));

	if (MTLogEnabled())
	{
		const AGameStateBase* GS = GameState;
		UE_LOG(LogMT, Log, TEXT("[MTMatch] HandleMatchHasStarted NetMode=%d Players=%d"),
			(int32)GetNetMode(), GS ? GS->PlayerArray.Num() : -1);
	}
	// TODO: 입력 권한 부여. 클라는 GameState->IsMatchInProgress()로 게이트.

	// 1. 매치 타이머 초기화 및 시작
    RemainingTime = MatchDuration;
    
    // AMTGameState가 있다면 현재 남은 시간을 클라이언트들에게 동기화
    if (AMTGameState* GS = GetGameState<AMTGameState>())
    {
        // GS->SetRemainingTime(RemainingTime); // GameState에 구현 필요
    }

    GetWorldTimerManager().SetTimer(MatchTimerHandle, this, &AMTMatchGameMode::UpdateMatchTimer, 1.0f, true);

    // 2. 기획서: 행인 랜덤 스폰 루프 시작
    SpawnInitialPedestrians();
}
