#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "MTMatchGameMode.generated.h"

/** 인게임 매치 — bDelayedStart로 대기하다가 전원 로딩 완료 시 StartMatch. */
UCLASS()
class MEOWTRACTIVE_API AMTMatchGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AMTMatchGameMode();

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void HandleSeamlessTravelPlayer(AController*& C) override;

protected:
	// 전원 로딩 완료 + 이동 중 플레이어 없음 → true 시 StartMatch
	virtual bool ReadyToStartMatch_Implementation() override;
	virtual void HandleMatchHasStarted() override;

#pragma region Rule

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Match Rules")
    int32 MatchDuration = 180; // 라운드 시간

    UPROPERTY(EditDefaultsOnly, Category = "Match Rules")
    TSubclassOf<class AMTPedestrianBase> NormalPedestrianClass; // 행인

    UPROPERTY(EditDefaultsOnly, Category = "Match Rules")
    int32 MaxNormalPedestrians = 20;

private:
    FTimerHandle MatchTimerHandle;
    int32 RemainingTime;

    void UpdateMatchTimer();
    void SpawnInitialPedestrians();

#pragma endregion

private:
	// 컨트롤러의 MTPlayerState를 로딩 완료로 표시
	void MarkLoaded(AController* C);
};
