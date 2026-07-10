#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Game/MTTypes.h"
#include "MTLobbyGameState.generated.h"

// 방 설정 변경 알림 (로비 UI가 구독 → 맵/모드 텍스트 갱신)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMTOnRoomSettingsChanged);

// 자동 시작 카운트다운 변경 (-1=없음, 0~N=남은 초). UI가 구독
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMTOnStartCountdownChanged, int32, RemainingSeconds);

/** 로비 방 설정 복제 — 모든 클라이언트가 방이름/모드/맵을 표시할 수 있게 한다. */
UCLASS()
class MEOWTRACTIVE_API AMTLobbyGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 서버 전용 — LobbyGameMode가 호출
	void SetRoomSettings(const FMTRoomSettings& NewSettings);

	UFUNCTION(BlueprintPure, Category = "MT|Lobby")
	FString GetRoomName() const { return RoomName; }

	UFUNCTION(BlueprintPure, Category = "MT|Lobby")
	EMTRoomGameMode GetRoomGameMode() const { return RoomGameMode; }

	UFUNCTION(BlueprintPure, Category = "MT|Lobby")
	EMTRoomMap GetRoomMap() const { return RoomMap; }

	UFUNCTION(BlueprintPure, Category = "MT|Lobby")
	bool HasRoomPassword() const { return bHasPassword; }

	// 자동 시작 카운트다운 (서버 전용 setter, BP 읽기)
	void SetStartCountdown(int32 Seconds);

	UFUNCTION(BlueprintPure, Category = "MT|Lobby")
	int32 GetStartCountdown() const { return StartCountdown; }

	UPROPERTY(BlueprintAssignable, Category = "MT|Lobby")
	FMTOnRoomSettingsChanged OnRoomSettingsChanged;

	UPROPERTY(BlueprintAssignable, Category = "MT|Lobby")
	FMTOnStartCountdownChanged OnStartCountdownChanged;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_RoomSettings)
	FString RoomName;

	UPROPERTY(ReplicatedUsing = OnRep_RoomSettings)
	EMTRoomGameMode RoomGameMode = EMTRoomGameMode::FFA;

	UPROPERTY(ReplicatedUsing = OnRep_RoomSettings)
	EMTRoomMap RoomMap = EMTRoomMap::Insadong;

	UPROPERTY(ReplicatedUsing = OnRep_RoomSettings)
	bool bHasPassword = false;

	// -1 = 카운트다운 없음, 0~N = 자동 시작까지 남은 초
	UPROPERTY(ReplicatedUsing = OnRep_Countdown)
	int32 StartCountdown = -1;

	UFUNCTION()
	void OnRep_RoomSettings();

	UFUNCTION()
	void OnRep_Countdown();
};
