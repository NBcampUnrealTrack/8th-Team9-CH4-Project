#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Game/MTTypes.h"
#include "MTLobbyWidget.generated.h"

class AMTPlayerController;
class AMTPlayerState;

// UI 새로고침 신호 (BP가 바인딩 → 로스터/버튼 갱신)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMTOnLobbyRefresh);

/** 로비 위젯: 표현 + 의도 전달 + 읽기만. 규칙·트래블은 GameMode/Flow가 담당. */
UCLASS()
class MEOWTRACTIVE_API UMTLobbyWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// 로비 진입 시 호출 (입력 모드 + 주기적 새로고침 시작)
	UFUNCTION(BlueprintCallable, Category = "MT|Lobby")
	void LobbySetup();

	// --- 버튼 → 의도 전달 ---
	UFUNCTION(BlueprintCallable, Category = "MT|Lobby")
	void SelectCat(EMTCatType CatType);

	UFUNCTION(BlueprintCallable, Category = "MT|Lobby")
	void ToggleReady();

	UFUNCTION(BlueprintCallable, Category = "MT|Lobby")
	void RequestStart();

	UFUNCTION(BlueprintCallable, Category = "MT|Lobby")
	void LeaveLobby();

	// --- 표시용 읽기 ---
	UFUNCTION(BlueprintPure, Category = "MT|Lobby")
	TArray<AMTPlayerState*> GetPlayers() const;

	// 로컬 플레이어의 PlayerState (슬롯 비교용)
	UFUNCTION(BlueprintPure, Category = "MT|Lobby")
	AMTPlayerState* GetLocalPlayerState() const;

	// 고정 슬롯 수 (4칸 레이아웃 — BP가 0..N 루프)
	UFUNCTION(BlueprintPure, Category = "MT|Lobby")
	int32 GetMaxSlots() const { return MaxSlots; }

	// 해당 슬롯의 플레이어 (없으면 nullptr = "대기 중")
	UFUNCTION(BlueprintPure, Category = "MT|Lobby")
	AMTPlayerState* GetPlayerInSlot(int32 SlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "MT|Lobby")
	bool IsLocalReady() const;

	UFUNCTION(BlueprintPure, Category = "MT|Lobby")
	EMTCatType GetLocalSelectedCat() const;

	UFUNCTION(BlueprintPure, Category = "MT|Lobby")
	bool IsHost() const;            // 시작 버튼 표시용

	UFUNCTION(BlueprintPure, Category = "MT|Lobby")
	bool CanStart() const;          // 시작 버튼 활성용(전원 준비) — 표시 힌트, 강제는 서버

	// BP가 바인딩해서 UI 새로고침
	UPROPERTY(BlueprintAssignable, Category = "MT|Lobby")
	FMTOnLobbyRefresh OnLobbyRefresh;

protected:
	virtual void NativeDestruct() override;
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

private:
	AMTPlayerController* GetMTPC() const;
	AMTPlayerState* GetLocalMTPS() const;

	UFUNCTION()
	void HandleRefresh();           // 타이머 → OnLobbyRefresh 방송

	FTimerHandle RefreshTimer;

	UPROPERTY(EditAnywhere, Category = "MT|Lobby")
	int32 MaxSlots = 4;
};
