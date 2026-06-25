#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "MTWanderStateTreeTask.generated.h"

class AActor;

USTRUCT()
struct FMTWanderTaskInstanceData
{
	GENERATED_BODY()

	// 행인 (Context Actor 바인딩)
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY()
	FVector Destination = FVector::ZeroVector;

	UPROPERTY()
	bool bWaiting = false;

	UPROPERTY()
	float WaitElapsed = 0.f;
};

/** 배회 태스크: 랜덤 점 찾기 → 이동 → 도착 후 대기 → 반복. (절대 완료 안 함 → Attracted 전환이 빼냄) */
USTRUCT(meta = (DisplayName = "Pedestrian Wander"))
struct MEOWTRACTIVE_API FMTWanderStateTreeTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMTWanderTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	// 랜덤 점 탐색 반경
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float WanderRadius = 1500.f;

	// 도착 판정 반경
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float AcceptanceRadius = 100.f;

	// 도착 후 대기 시간 (초)
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float WaitTime = 2.f;
};
