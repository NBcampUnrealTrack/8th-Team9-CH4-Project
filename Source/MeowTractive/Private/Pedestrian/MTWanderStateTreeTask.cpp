#include "Pedestrian/MTWanderStateTreeTask.h"

#include "StateTreeExecutionContext.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "GameFramework/Pawn.h"

// 랜덤 reachable 점을 찾아 그 즉시 MoveTo. 성공 시 OutDest 갱신.
static bool PickAndMove(AActor* Actor, float Radius, float AcceptRadius, FVector& OutDest)
{
	const APawn* Pawn = Cast<APawn>(Actor);
	AAIController* AI = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
	if (!AI)
	{
		return false;
	}

	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(Actor->GetWorld());
	if (!Nav)
	{
		return false;
	}

	FNavLocation Loc;
	if (!Nav->GetRandomReachablePointInRadius(Actor->GetActorLocation(), Radius, Loc))
	{
		return false;
	}

	OutDest = Loc.Location;
	AI->MoveToLocation(OutDest, AcceptRadius, true, true, true, false);
	return true;
}

EStateTreeRunStatus FMTWanderStateTreeTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.bWaiting = false;
	Data.WaitElapsed = 0.f;
	PickAndMove(Data.Actor, WanderRadius, AcceptanceRadius, Data.Destination);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FMTWanderStateTreeTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (!Data.Actor)
	{
		return EStateTreeRunStatus::Running;
	}

	// 도착 후 대기 → 시간 지나면 다음 점으로
	if (Data.bWaiting)
	{
		Data.WaitElapsed += DeltaTime;
		if (Data.WaitElapsed >= WaitTime)
		{
			Data.bWaiting = false;
			PickAndMove(Data.Actor, WanderRadius, AcceptanceRadius, Data.Destination);
		}
		return EStateTreeRunStatus::Running;
	}

	// 이동 중: 도착 판정
	const float DistSq = FVector::DistSquared2D(Data.Actor->GetActorLocation(), Data.Destination);
	if (DistSq <= FMath::Square(AcceptanceRadius))
	{
		Data.bWaiting = true;
		Data.WaitElapsed = 0.f;
	}

	return EStateTreeRunStatus::Running; // 계속 배회 (완료 X) → Attracted 전환이 빼냄
}
