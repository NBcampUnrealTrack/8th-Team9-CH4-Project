#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "StateTreeConditionBase.h"
#include "MTAttractedStateTreeTask.generated.h"

class AActor;

/** 태스크 인스턴스 데이터 (상태별 런타임 값) */
USTRUCT()
struct FMTAttractedTaskInstanceData
{
	GENERATED_BODY()

	// 행인 (StateTree에서 Context Actor에 바인딩)
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY()
	float Elapsed = 0.f;
};

/** 매료 반응 태스크: Duration(기본 3초) 유지 후 행인 EndAttracted() 호출하고 성공 종료. */
USTRUCT(meta = (DisplayName = "Pedestrian Attracted (3s -> EndAttracted)"))
struct MEOWTRACTIVE_API FMTAttractedStateTreeTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMTAttractedTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	// 매료 응시 유지 시간 (초)
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float Duration = 3.f;
};

/** 조건 인스턴스 데이터 */
USTRUCT()
struct FMTIsAttractedConditionInstanceData
{
	GENERATED_BODY()

	// 행인 (Context Actor 바인딩)
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<AActor> Actor = nullptr;
};

/** Wander → Attracted 전환 조건: 행인이 매료 상태인가? */
USTRUCT(meta = (DisplayName = "Pedestrian Is Attracted"))
struct MEOWTRACTIVE_API FMTIsAttractedCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMTIsAttractedConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
