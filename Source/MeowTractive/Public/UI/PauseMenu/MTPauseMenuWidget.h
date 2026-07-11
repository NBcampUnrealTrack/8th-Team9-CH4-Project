#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MTPauseMenuWidget.generated.h"

class UButton;
class UMTSettingsWidget;

/** ESC 일시정지 메뉴 — 표현/버튼만. 열기/닫기·입력모드는 AMTPlayerController가 관리. */
UCLASS()
class MEOWTRACTIVE_API UMTPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	// UIOnly라 PC ESC가 안 오므로 위젯이 직접 닫기 처리
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ResumeButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> LeaveButton;

	// 설정 열기 (WBP에 SettingsButton 추가 시 자동 바인딩)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SettingsButton;

	// 설정 화면 위젯 (WBP_MTSettings)
	UPROPERTY(EditDefaultsOnly, Category = "MT|UI")
	TSubclassOf<UMTSettingsWidget> SettingsWidgetClass;

private:
	UFUNCTION()
	void HandleResume();

	UFUNCTION()
	void HandleLeave();

	UFUNCTION()
	void HandleSettings();

	// 공통 닫기 (PC로 위임 → 위젯 제거 + 입력모드 복원)
	void CloseMenu();
};
