#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Game/MTTypes.h"
#include "MTLobbyGameState.generated.h"

// 방 설정 변경 알림 (로비 UI가 구독 → 맵/모드 텍스트 갱신)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMTOnRoomSettingsChanged);

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

	UPROPERTY(BlueprintAssignable, Category = "MT|Lobby")
	FMTOnRoomSettingsChanged OnRoomSettingsChanged;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_RoomSettings)
	FString RoomName;

	UPROPERTY(ReplicatedUsing = OnRep_RoomSettings)
	EMTRoomGameMode RoomGameMode = EMTRoomGameMode::FFA;

	UPROPERTY(ReplicatedUsing = OnRep_RoomSettings)
	EMTRoomMap RoomMap = EMTRoomMap::Insadong;

	UPROPERTY(ReplicatedUsing = OnRep_RoomSettings)
	bool bHasPassword = false;

	UFUNCTION()
	void OnRep_RoomSettings();
};
