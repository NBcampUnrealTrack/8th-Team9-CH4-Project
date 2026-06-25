#include "Game/MTMatchGameMode.h"
#include "Game/MTGameState.h"
#include "Player/MTPlayerState.h"
#include "Player/MTPlayerController.h"
#include "Pedestrian/MTPedestrianBase.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "UObject/ConstructorHelpers.h"

AMTMatchGameMode::AMTMatchGameMode()
{
	bDelayedStart = true;        // 전원 로딩 전까지 매치 보류
	bUseSeamlessTravel = true;

	GameStateClass = AMTGameState::StaticClass();
	PlayerStateClass = AMTPlayerState::StaticClass();
	PlayerControllerClass = AMTPlayerController::StaticClass();

	// 임시 테스트용 폰 (추후 고양이 캐릭터로 교체)
	static ConstructorHelpers::FClassFinder<APawn> PawnBP(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PawnBP.Succeeded())
	{
		DefaultPawnClass = PawnBP.Class;
	}
}

void AMTMatchGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    MarkLoaded(NewPlayer);

    AMTPlayerState* MTPS = NewPlayer->GetPlayerState<AMTPlayerState>();
    if (!MTPS) return;

    const int32 Slot = GameState->PlayerArray.Num() - 1;
    MTPS->SetPlayerSlot(Slot);

    const TArray<FLinearColor> SlotColors = {
        FLinearColor(1.f, 0.2f, 0.2f),
        FLinearColor(0.2f, 0.4f, 1.f),
        FLinearColor(0.2f, 0.8f, 0.2f),
        FLinearColor(1.f, 0.8f, 0.f),
    };

    if (SlotColors.IsValidIndex(Slot))
    {
        MTPS->SetTeamColor(SlotColors[Slot]);
    }
}

void AMTMatchGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
	Super::HandleSeamlessTravelPlayer(C);
	MarkLoaded(C);                  // seamless travel 완료 = 새 맵 로딩 완료
}

void AMTMatchGameMode::HandleMatchHasEnded()
{
    Super::HandleMatchHasEnded();

    AMTGameState* GS = GetGameState<AMTGameState>();
    if (!GS) return;

    for (APlayerState* PS : GS->PlayerArray)
    {
        if (AMTPlayerController* PC = Cast<AMTPlayerController>(PS->GetPlayerController()))
        {
            PC->ClientShowResult();
        }
    }

    FTimerHandle LobbyTimer;
    GetWorldTimerManager().SetTimer(LobbyTimer, this, &AMTMatchGameMode::ReturnToLobby, 5.f, false);
}

void AMTMatchGameMode::ReturnToLobby()
{
	// 로비 복귀
	// GetWorld()->ServerTravel(TEXT("/Game/Maps/LobbyMap?listen"));
}

void AMTMatchGameMode::UpdateMatchTimer()
{
    RemainingTime--;

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
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

        if (APawn* SpawnedPed = GetWorld()->SpawnActor<APawn>(NormalPedestrianClass, SpawnLocation, SpawnRotation, SpawnParams))
        {
            SpawnedPed->SpawnDefaultController();
        }
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
