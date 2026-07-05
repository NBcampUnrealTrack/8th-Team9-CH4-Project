// MTGameState.cpp
#include "Game/MTGameState.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "Pedestrian/MTAttractiveComponent.h"
#include "Pedestrian/MTPedestrianBase.h"

void AMTGameState::AddPlayerState(APlayerState* PlayerState)
{
    Super::AddPlayerState(PlayerState);

    if (!HasAuthority() || !PlayerState)
    {
        return;
    }

    for (TActorIterator<AMTPedestrianBase> It(GetWorld()); It; ++It)
    {
        if (UMTAttractiveComponent* AttractiveComponent = It->GetAttractiveComponent())
        {
            AttractiveComponent->AddPlayerState(PlayerState);
        }
    }
}

void AMTGameState::RemovePlayerState(APlayerState* PlayerState)
{
    Super::RemovePlayerState(PlayerState);

    if (!HasAuthority() || !PlayerState)
    {
        return;
    }

    for (TActorIterator<AMTPedestrianBase> It(GetWorld()); It; ++It)
    {
        if (UMTAttractiveComponent* AttractiveComponent = It->GetAttractiveComponent())
        {
            AttractiveComponent->RemovePlayerState(PlayerState);
        }
    }
}

void AMTGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AMTGameState, PlayerScores);
}

void AMTGameState::AddAttractedCount(APlayerState* TargetPlayerState)
{
    if (!TargetPlayerState || !HasAuthority()) return;

    FPlayerScore* Entry = PlayerScores.FindByPredicate(
        [TargetPlayerState](const FPlayerScore& Item)
        {
            return Item.PlayerState == TargetPlayerState;
        }
    );

    if (Entry)
    {
        Entry->AttractedCount++;
    }
    else
    {
        FPlayerScore NewEntry;
        NewEntry.PlayerState = TargetPlayerState;
        NewEntry.AttractedCount = 1;
        PlayerScores.Add(NewEntry);
    }

    OnRep_PlayerScores();
}

int32 AMTGameState::GetAttractedCount(APlayerState* TargetPlayerState) const
{
    const FPlayerScore* Entry = PlayerScores.FindByPredicate(
        [TargetPlayerState](const FPlayerScore& Item)
        {
            return Item.PlayerState == TargetPlayerState;
        }
    );
    return Entry ? Entry->AttractedCount : 0;
}

void AMTGameState::OnRep_PlayerScores()
{
    // UI 갱신 필요하면 여기서
}

void AMTGameState::RemoveAttractedCount(APlayerState* TargetPlayerState)
{
    if (!TargetPlayerState || !HasAuthority()) return;

    FPlayerScore* Entry = PlayerScores.FindByPredicate(
        [TargetPlayerState](const FPlayerScore& Item)
        {
            return Item.PlayerState == TargetPlayerState;
        }
    );

    if (Entry)
    {
        Entry->AttractedCount = FMath::Max(0, Entry->AttractedCount - 1);
        OnRep_PlayerScores();
    }
}
