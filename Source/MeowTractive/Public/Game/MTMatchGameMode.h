#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Pedestrian/MTPedestrianAppearanceTypes.h"
#include "Game/MTTypes.h"
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

	// 매치 맵 진입 후 신규 접속 거부 (인게임 중도 참여 차단). 로비 동반 플레이어는 seamless라 안 거침.
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;

	virtual void HandleMatchHasEnded() override;

	// 세션 유지 + 전원 로비 맵으로 복귀 (호스트 권위 ServerTravel). 대상 맵은 UMTGameInstance::LobbyMap 단일 출처.
	void ReturnToLobby();

	// 결과 화면 자동 복귀 타임아웃(초). 이 시간 동안 버튼 미입력 시 자동으로 로비 복귀.
	UPROPERTY(EditDefaultsOnly, Category = "Match Rules")
	float ResultScreenDuration = 60.f;

	// 슬롯별 팀색 — 로비 미경유(PIE 직행) 폴백용. 정식 배정은 AMTLobbyGameMode::TeamColors (동일 팔레트 유지)
	UPROPERTY(EditDefaultsOnly, Category = "Match Rules")
	TArray<FLinearColor> TeamColors;

	// 선택 고양이별 폰 클래스 (BP_Fatty/Mackerel/Spotted). 없으면 DefaultPawnClass 폴백.
	UPROPERTY(EditDefaultsOnly, Category = "Match Rules")
	TMap<EMTCatType, TSubclassOf<APawn>> CatPawnClasses;

	// 선택 고양이에 따라 스폰 폰 클래스 결정
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	// 플레이어 스타트 지점
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	// 리스폰 요청
	void RequestRespawn(AController* Controller);

	float GetRespawnDelay() const { return RespawnDelay; }

protected:
	// 전원 로딩 완료 → 시작 카운트다운(3·2·1) 경과 후 true 시 StartMatch
	virtual bool ReadyToStartMatch_Implementation() override;
	virtual void HandleMatchHasStarted() override;

	// 전원 로딩 완료 후 매치 시작까지 카운트다운(초) — GameState로 복제돼 HUD가 표시
	UPROPERTY(EditDefaultsOnly, Category = "Match Rules", meta = (ClampMin = "0"))
	int32 StartCountdownSeconds = 3;

#pragma region Rule

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Match Rules")
    int32 MatchDuration = 180; // 라운드 시간

    UPROPERTY(EditDefaultsOnly, Category = "Match Rules")
    TSubclassOf<class AMTPedestrianBase> NormalPedestrianClass; // 행인

	UPROPERTY(EditDefaultsOnly, Category = "Match Rules|Pedestrian")
	FMTPedestrianGenerationConfig PedestrianGenerationConfig;

	UPROPERTY(EditDefaultsOnly, Category = "Match Rules")
    int32 MaxNormalPedestrians = 20;

	// MainStreet 기준 행인 NavMesh 랜덤 스폰 반경(cm).
	UPROPERTY(EditDefaultsOnly, Category = "Match Rules|Pedestrian", meta = (ClampMin = "0.0"))
	float PedestrianSpawnRadius = 2000.f;

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

	// 매치 시작 전(대기·카운트다운) 시야/이동 입력 잠금.
	// 해제는 StartMatch의 폰 스폰 때 엔진이 ClientRestart에서 ResetIgnoreInputFlags로 처리한다.
	void LockPreMatchInput(AController* C);

	// 팀색 폴백 — 로비에서 배정돼 운반된 색은 유지, 로비 미경유(PIE 직접 진입)만 진입 순서로 배정.
	void AssignTeamColor(AController* C);

	int32 NextColorSlot = 0;   // 슬롯 미배정 시 폴백 인덱스

	// 시작 카운트다운 1초 감소 — 0 도달 시 ReadyToStartMatch가 true
	void TickStartCountdown();

	FTimerHandle StartCountdownTimer;
	bool bStartCountdownRunning = false;
	bool bStartCountdownFinished = false;
};
