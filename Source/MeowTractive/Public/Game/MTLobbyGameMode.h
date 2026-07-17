#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Game/MTTypes.h"
#include "MTLobbyGameMode.generated.h"

UCLASS()
class MEOWTRACTIVE_API AMTLobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMTLobbyGameMode();

	virtual void BeginPlay() override;
	virtual void Logout(AController* Exiting) override;

	// 폰 스폰 직전 훅 — 신규 접속·매치 복귀(seamless) 양쪽이 여기로 모인다.
	// 슬롯/고양이가 정해진 뒤에 스폰돼야 하므로 Super보다 먼저 셋업한다.
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	// 전원(호스트 포함) 준비 완료 + 최소 2명
	bool CanStartMatch() const;

	// 호스트 시작 요청 처리 (조건 충족 시 다음 맵으로 ServerTravel)
	void TryStartMatch();

	// 개발용 강제 시작 — 인원/준비 조건 무시 (콘솔 `MT.Start`가 호출)
	void ForceStartMatch();

	// Ready 상태 변경 시 PlayerController가 호출 → 전원 준비면 자동 시작 카운트다운
	void NotifyReadyChanged();

	// 선택 고양이별 조종 폰 (BP_Fatty/Spotted/Mackerel — 인게임과 동일). 없으면 DefaultPawnClass.
	UPROPERTY(EditDefaultsOnly, Category = "MT|Lobby")
	TMap<EMTCatType, TSubclassOf<APawn>> CatPawnClasses;

	// 무작위 초기 선택 후보 — 미선택 플레이어에게 이 중 하나를 배정한다.
	// 고정값이면 "0번 펀치 = 그 고양이"로 특정되므로 랜덤. 비면 CatPawnClasses 키에서 뽑는다.
	UPROPERTY(EditDefaultsOnly, Category = "MT|Lobby")
	TArray<EMTCatType> RandomStartCats;

	// 슬롯별 팀색 (인덱스 = PlayerSlot). 빨·파·초·노 순 — CopyProperties로 매치까지 운반됨
	UPROPERTY(EditDefaultsOnly, Category = "MT|Lobby")
	TArray<FLinearColor> TeamColors;

	// 선택 고양이에 따라 로비 조종 폰 결정
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	// 슬롯별 PlayerStart("Slot%d")로 스폰 → 자기 조형물 앞에 배치
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	// 방 설정 변경 — 호스트(리슨 로컬)만 허용. 접속 플레이어는 유지된다.
	void ApplyRoomSettings(AController* Requestor, FMTRoomSettings NewSettings);

	// 강퇴 — 호스트만, 호스트 자신은 불가
	void KickPlayer(AController* Requestor, APlayerState* Target);

	// 로비에서 고양이 선택 변경 시 호출 → 새 종류 폰으로 재스폰
	void RespawnLobbyPawn(AController* C);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "MT|Lobby")
	int32 MaxPlayers = 4;

	// 매치 맵 폴백 (맵 선택 테이블에 없을 때). 쿠킹 추적되게 BP에서 지정 권장.
	UPROPERTY(EditDefaultsOnly, Category = "MT|Lobby")
	TSoftObjectPtr<UWorld> FallbackMatchMap;

	// 맵 선택 → 실제 맵 (Random 제외. 새 맵은 여기에 추가)
	UPROPERTY(EditDefaultsOnly, Category = "MT|Lobby")
	TMap<EMTRoomMap, TSoftObjectPtr<UWorld>> MatchMaps;

private:
	// 슬롯 배정/재사용 + 팀색 설정 (PostLogin·SeamlessTravel 공통)
	void SetupLobbyPlayer(AController* C);

	// 방 설정에서 매치 맵 경로 결정 (Random이면 테이블에서 무작위)
	FString ResolveMatchMapPath() const;

	int32 AcquireSlot();
	void ReleaseSlot(int32 Slot);

	// 1초마다 호출 — 카운트다운 감소, 0이면 시작
	void TickCountdown();

	TArray<bool> UsedSlots;     // 슬롯 점유 현황 (크기 = MaxPlayers)

	FTimerHandle CountdownTimer;

	// 전원 준비 후 시작까지 카운트다운(초) — 그 사이 준비 해제/이탈하면 취소
	UPROPERTY(EditDefaultsOnly, Category = "MT|Lobby")
	int32 AutoStartSeconds = 3;

	// 카운트다운 0("게임시작!") 후 트래블까지 대기(초) — HUD의 문구 노출(0.5)+페이드아웃(1.0)과 합 맞춤
	UPROPERTY(EditDefaultsOnly, Category = "MT|Lobby", meta = (ClampMin = "0.0"))
	float StartTravelDelay = 1.5f;

	FTimerHandle TravelDelayTimer;
};
