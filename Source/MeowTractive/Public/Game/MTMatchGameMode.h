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

	virtual void HandleMatchHasEnded() override;

	// 세션 유지 + 전원 로비 맵으로 복귀 (호스트 권위 ServerTravel)
	void ReturnToLobby();

	// 복귀 대상 로비 맵
	UPROPERTY(EditDefaultsOnly, Category = "Match Rules")
	FString LobbyMapPath = TEXT("/Game/Main/Maps/Lobby");

	// 결과 화면 자동 복귀 타임아웃(초). 이 시간 동안 버튼 미입력 시 자동으로 로비 복귀.
	UPROPERTY(EditDefaultsOnly, Category = "Match Rules")
	float ResultScreenDuration = 60.f;

	// 슬롯별 팀색 (인덱스 = PlayerSlot). 빨·파·초·노 순
	UPROPERTY(EditDefaultsOnly, Category = "Match Rules")
	TArray<FLinearColor> TeamColors;

	// 플레이어 스타트 지점
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	// 리스폰 요청
	void RequestRespawn(AController* Controller);

	float GetRespawnDelay() const { return RespawnDelay; }

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

	TArray<class APlayerStart*> UsedStarts; // 플레이어 스타트 지점

	UPROPERTY(EditDefaultsOnly, Category = "Match Rules")
    float RespawnDelay = 5.f; // 리스폰 쿨타임

	TSet<AController*> RespawningControllers; // 리스폰하는 컨트롤러

private:
    FTimerHandle MatchTimerHandle;
    FTimerHandle LobbyTimer;        // 결과 화면 자동 복귀 타이머
    int32 RemainingTime;

    void UpdateMatchTimer();
    void SpawnInitialPedestrians();

#pragma endregion

private:
	// 컨트롤러의 MTPlayerState를 로딩 완료로 표시
	void MarkLoaded(AController* C);

	// 슬롯 기준 팀색 부여. 로비를 안 거쳐 슬롯이 없으면(PIE 직접 진입) 진입 순서로 폴백.
	void AssignTeamColor(AController* C);

	int32 NextColorSlot = 0;   // 슬롯 미배정 시 폴백 인덱스
};
