#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MTLobbyPlayerEntry.generated.h"

class AMTPlayerState;
class UMTLobbyHUDWidget;
class UImage;
class UBorder;

/** 접속자 목록 엔트리 — 아바타/Ready테두리를 C++이 세팅. BP는 위젯 배치만. 강퇴는 Tab 강퇴창에서 처리. */
UCLASS()
class MEOWTRACTIVE_API UMTLobbyPlayerEntry : public UUserWidget
{
	GENERATED_BODY()

public:
	// HUD의 RefreshUI에서 각 엔트리에 호출
	UFUNCTION(BlueprintCallable, Category = "MT|LobbyHUD")
	void Setup(AMTPlayerState* PS, UMTLobbyHUDWidget* Owner);

protected:
	virtual void NativeDestruct() override;

	// WBP 위젯 이름과 일치해야 바인딩됨 (없으면 무시)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> SteamProfile;   // 아바타

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ReadyBorder;   // CommonBorder도 UBorder라 바인딩됨

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ReadyCheck;    // 초록 체크 아이콘 (선택)

	// Ready 테두리 색 (에디터 조정 가능)
	UPROPERTY(EditAnywhere, Category = "MT|LobbyHUD")
	FLinearColor ReadyBorderColor = FLinearColor(0.f, 1.f, 0.f, 1.f);

	UPROPERTY(EditAnywhere, Category = "MT|LobbyHUD")
	FLinearColor NotReadyBorderColor = FLinearColor(1.f, 1.f, 1.f, 0.f);

private:
	UPROPERTY()
	TObjectPtr<AMTPlayerState> SlotPS;

	UPROPERTY()
	TObjectPtr<UMTLobbyHUDWidget> OwnerHUD;

	void RefreshVisuals();
	void TryLoadAvatar();

	FTimerHandle AvatarRetryTimer;
	int32 AvatarRetries = 0;
};
