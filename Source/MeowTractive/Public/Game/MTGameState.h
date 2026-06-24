#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "MTGameState.generated.h"

/** AGameState 기반 — MatchState(WaitingToStart/InProgress) 복제를 제공.
 *  클라는 IsMatchInProgress()로 입력/UI를 게이트. 로스터·남은시간 등은 추후 확장. */
UCLASS()
class MEOWTRACTIVE_API AMTGameState : public AGameState
{
	GENERATED_BODY()
};
