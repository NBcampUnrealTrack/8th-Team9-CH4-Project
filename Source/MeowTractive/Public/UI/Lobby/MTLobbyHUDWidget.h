#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/MTTypes.h"
#include "MTLobbyHUDWidget.generated.h"

class AMTPlayerState;
class AMTLobbyGameState;
class AMTPlayerController;
class UTexture2D;

// 목록/상태 갱신 신호 (BP가 구독해 위젯 리프레시)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMTOnLobbyHUDRefresh);
// 카운트다운 표시 갱신 (음수=숨김, 0=시작, N=남은 초)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMTOnLobbyCountdown, int32, RemainingSeconds);

/** 로비 HUD 베이스 — 데이터/판정은 여기서, 레이아웃만 WBP에서. 입력은 폰이 관리(비차단). */
UCLASS()
class MEOWTRACTIVE_API UMTLobbyHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// --- 접속자 목록 ---
	UFUNCTION(BlueprintPure, Category = "MT|LobbyHUD")
	TArray<AMTPlayerState*> GetPlayers() const;

	UFUNCTION(BlueprintPure, Category = "MT|LobbyHUD")
	AMTPlayerState* GetLocalPlayerState() const;

	// --- 방 정보 (복제값) ---
	UFUNCTION(BlueprintPure, Category = "MT|LobbyHUD")
	FString GetRoomName() const;

	UFUNCTION(BlueprintPure, Category = "MT|LobbyHUD")
	EMTRoomGameMode GetRoomGameMode() const;

	UFUNCTION(BlueprintPure, Category = "MT|LobbyHUD")
	EMTRoomMap GetRoomMap() const;

	// "방이름 | 개인전 | 인사동" 한 줄 요약 (enum→표시명 변환 포함)
	UFUNCTION(BlueprintPure, Category = "MT|LobbyHUD")
	FText GetRoomSummaryText() const;

	// --- 판정 헬퍼 ---
	UFUNCTION(BlueprintPure, Category = "MT|LobbyHUD")
	bool IsLocalReady() const;

	UFUNCTION(BlueprintPure, Category = "MT|LobbyHUD")
	bool IsLocalHost() const;

	// 목록 엔트리에서: 이 대상에 강퇴 X를 보여줄지 (내가 호스트 && 대상≠나)
	UFUNCTION(BlueprintPure, Category = "MT|LobbyHUD")
	bool CanKick(AMTPlayerState* Target) const;

	// 준비 버튼 라벨/색 헬퍼
	UFUNCTION(BlueprintPure, Category = "MT|LobbyHUD")
	FText GetReadyButtonText() const;

	// --- 액션 (버튼 → 서버 RPC 래핑) ---
	UFUNCTION(BlueprintCallable, Category = "MT|LobbyHUD")
	void ToggleReady();

	UFUNCTION(BlueprintCallable, Category = "MT|LobbyHUD")
	void KickPlayer(AMTPlayerState* Target);

	UFUNCTION(BlueprintCallable, Category = "MT|LobbyHUD")
	void LeaveLobby();

	// 스팀 아바타 (없으면 nullptr → BP가 기본 아이콘/재시도)
	UFUNCTION(BlueprintCallable, Category = "MT|LobbyHUD")
	UTexture2D* GetAvatar(AMTPlayerState* Target) const;

	// BP가 바인딩 → 목록/카운트다운 갱신
	UPROPERTY(BlueprintAssignable, Category = "MT|LobbyHUD")
	FMTOnLobbyHUDRefresh OnRefresh;

	UPROPERTY(BlueprintAssignable, Category = "MT|LobbyHUD")
	FMTOnLobbyCountdown OnCountdown;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	AMTLobbyGameState* GetLobbyGS() const;
	AMTPlayerController* GetMTPC() const;

	// GameState/PlayerState 델리게이트 → OnRefresh/OnCountdown로 전달
	UFUNCTION()
	void HandleRoomSettingsChanged();

	UFUNCTION()
	void HandleCountdownChanged(int32 RemainingSeconds);

	// 사람 들락날락/준비 변화 폴백 (0.4s)
	UFUNCTION()
	void PollRefresh();

	FTimerHandle RefreshTimer;
};
