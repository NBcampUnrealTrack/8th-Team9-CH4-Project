#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Game/MTTypes.h"
#include "MTPlayerController.generated.h"

class UInputAction;

UCLASS()
class MEOWTRACTIVE_API AMTPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// 로비 버튼 → 서버 요청 (BP UI에서 호출)
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "MT|Lobby")
	void Server_SetSelectedCat(EMTCatType NewCat);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "MT|Lobby")
	void Server_ToggleReady();

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "MT|Lobby")
	void Server_RequestStartMatch();

	// 결과 화면 "로비로" 버튼 → 세션 유지 + 전원 로비 복귀 (호스트가 ServerTravel)
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "MT|Match")
	void Server_ReturnToLobby();

	// 로비 방 설정 변경 (호스트만 — GameMode가 검증)
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "MT|Lobby")
	void Server_UpdateRoomSettings(FMTRoomSettings NewSettings);

	// 강퇴 요청 (호스트만 — GameMode가 검증)
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "MT|Lobby")
	void Server_KickPlayer(APlayerState* TargetPlayer);

	// 강퇴 통보 → 메뉴 복귀 시 MessageDialog로 표시되게 사유 저장
	virtual void ClientWasKicked_Implementation(const FText& KickReason) override;

	UFUNCTION(Client, Reliable)
	void ClientShowResult();

	UPROPERTY(EditDefaultsOnly, Category = "MT|UI")
	TSubclassOf<class UMTMatchGameResultWidget> ResultWidgetClass;

	// ESC 일시정지 메뉴 토글 (열려 있으면 닫음). IA_Pause 바인딩·위젯이 호출. 로컬 전용.
	UFUNCTION(BlueprintCallable, Category = "MT|UI")
	void TogglePauseMenu();

	UPROPERTY(EditDefaultsOnly, Category = "MT|UI")
	TSubclassOf<class UMTPauseMenuWidget> PauseMenuClass;

	// 일시정지 토글 입력 (IA_Pause — ESC/P는 IMC에서 매핑)
	UPROPERTY(EditDefaultsOnly, Category = "MT|UI")
	TObjectPtr<UInputAction> PauseAction;

protected:
	virtual void SetupInputComponent() override;

private:
	float LastReadyToggleTime = -100.f;     // 서버 쿨다운 기준 시각
	static constexpr float ReadyCooldown = 0.5f;

	// 열려 있는 일시정지 메뉴 인스턴스 (없으면 nullptr = 닫힘)
	UPROPERTY(Transient)
	TObjectPtr<class UMTPauseMenuWidget> PauseMenu;
};
