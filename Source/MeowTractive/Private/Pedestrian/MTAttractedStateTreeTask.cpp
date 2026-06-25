#include "Pedestrian/MTAttractedStateTreeTask.h"

#include "Pedestrian/MTPedestrianBase.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FMTAttractedStateTreeTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.Elapsed = 0.f;
	return EStateTreeRunStatus::Running; // 계속 Tick
}

EStateTreeRunStatus FMTAttractedStateTreeTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.Elapsed += DeltaTime;

	if (Data.Elapsed >= Duration)
	{
		// 3초 경과 → 매료 해제(매료도 풀 복구) 후 상태 종료
		if (AMTPedestrianBase* Ped = Cast<AMTPedestrianBase>(Data.Actor))
		{
			Ped->EndAttracted();
		}
		return EStateTreeRunStatus::Succeeded;
	}
	return EStateTreeRunStatus::Running;
}

bool FMTIsAttractedCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	const AMTPedestrianBase* Ped = Cast<AMTPedestrianBase>(Data.Actor);
	return Ped && Ped->IsAttracted();
}
