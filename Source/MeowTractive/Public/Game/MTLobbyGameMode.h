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
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	// 매치→로비 복귀(seamless travel) 시에도 슬롯 셋업
	virtual void HandleSeamlessTravelPlayer(AController*& C) override;

	// 전원 준비 완료 + 최소 1명
	bool CanStartMatch() const;

	// 호스트 시작 요청 처리 (조건 충족 시 다음 맵으로 ServerTravel)
	void TryStartMatch();

	// 방 설정 변경 — 호스트(리슨 로컬)만 허용. 접속 플레이어는 유지된다.
	void ApplyRoomSettings(AController* Requestor, FMTRoomSettings NewSettings);

	// 강퇴 — 호스트만, 호스트 자신은 불가
	void KickPlayer(AController* Requestor, APlayerState* Target);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "MT|Lobby")
	int32 MaxPlayers = 4;

	// 매치 맵 폴백 (맵 선택 테이블에 없을 때)
	UPROPERTY(EditDefaultsOnly, Category = "MT|Lobby")
	FString NextMapPath = TEXT("/Game/Map/Map_Insa/Prototype_Insadong");

	// 맵 선택 → 실제 맵 경로 (Random 제외. 새 맵은 여기에 추가)
	UPROPERTY(EditDefaultsOnly, Category = "MT|Lobby")
	TMap<EMTRoomMap, FString> MatchMapPaths;

private:
	// 슬롯 배정/재사용 + 팀색 설정 (PostLogin·SeamlessTravel 공통)
	void SetupLobbyPlayer(AController* C);

	// 방 설정에서 매치 맵 경로 결정 (Random이면 테이블에서 무작위)
	FString ResolveMatchMapPath() const;

	int32 AcquireSlot();
	void ReleaseSlot(int32 Slot);

	TArray<bool> UsedSlots;     // 슬롯 점유 현황 (크기 = MaxPlayers)
};
