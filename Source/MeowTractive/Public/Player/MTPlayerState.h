#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Game/MTTypes.h"
#include "MTPlayerState.generated.h"

// 로비/매치 상태 변경 시 UI 갱신용 (BP 위젯이 구독)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMTOnLobbyStateChanged);

UCLASS()
class MEOWTRACTIVE_API AMTPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AMTPlayerState();

	// --- 서버 전용 setter (PlayerController RPC가 호출) ---
	void SetSelectedCat(EMTCatType NewCat);
	void SetReady(bool bNewReady);
	void SetPlayerSlot(int32 NewSlot);
	void SetLoaded(bool bNewLoaded);
	void SetHost(bool bNewHost);

	// --- BP/UI 읽기 ---
	UFUNCTION(BlueprintPure, Category = "MT|Lobby")
	EMTCatType GetSelectedCat() const { return SelectedCatType; }

	UFUNCTION(BlueprintPure, Category = "MT|Lobby")
	bool IsReady() const { return bIsReady; }

	UFUNCTION(BlueprintPure, Category = "MT|Lobby")
	int32 GetPlayerSlot() const { return PlayerSlot; }

	UFUNCTION(BlueprintPure, Category = "MT|Lobby")
	int32 GetTeamId() const { return TeamId; }

	// 팀 색
	UFUNCTION(BlueprintPure, Category = "MT|Player")
	FLinearColor GetTeamColor() const { return TeamColor; }

	void SetTeamColor(FLinearColor NewColor);

	// 방장 여부 (호스트 슬롯에 "게임시작" 표시용)
	UFUNCTION(BlueprintPure, Category = "MT|Lobby")
	bool IsHost() const { return bIsHost; }

	bool IsLoaded() const { return bIsLoaded; }

	// 로비 UI가 바인딩
	UPROPERTY(BlueprintAssignable, Category = "MT|Lobby")
	FMTOnLobbyStateChanged OnLobbyStateChanged;

	virtual void CopyProperties(APlayerState* NewPlayerState) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState)
	EMTCatType SelectedCatType = EMTCatType::None;

	UPROPERTY(ReplicatedUsing = OnRep_LobbyState)
	bool bIsReady = false;

	UPROPERTY(Replicated)
	int32 PlayerSlot = -1;     // 서버가 PostLogin에서 부여 (0~3)

	UPROPERTY(Replicated)
	int32 TeamId = -1;         // 개인전 = -1

	UPROPERTY(Replicated)
	bool bIsLoaded = false;    // 레벨마다 리셋 (CopyProperties로 복사 안 함)

	UPROPERTY(Replicated)
	bool bIsHost = false;      // 방장 여부

	UFUNCTION()
	void OnRep_LobbyState();

	// 서버에서 값 바꾼 직후 호스트(리슨 서버) UI도 갱신되게 직접 방송
	void BroadcastChanged();

	// 플레이어 팀 색
	UPROPERTY(Replicated)
	FLinearColor TeamColor = FLinearColor::White;
};
