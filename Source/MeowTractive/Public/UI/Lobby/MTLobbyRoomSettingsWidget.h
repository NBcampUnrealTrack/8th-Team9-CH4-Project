#pragma once

#include "CoreMinimal.h"
#include "UI/Lobby/MTLobbyConsoleWidget.h"
#include "MTLobbyRoomSettingsWidget.generated.h"

class UEditableTextBox;
class UComboBoxString;
class UButton;

/** 호스트 콘솔 — 방 설정 변경: 현재 설정 표시 → 적용 시 Server_UpdateRoomSettings. */
UCLASS()
class MEOWTRACTIVE_API UMTLobbyRoomSettingsWidget : public UMTLobbyConsoleWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> RoomNameInput;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> PasswordInput;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> GameModeCombo;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> MapCombo;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ApplyButton;

private:
	UFUNCTION()
	void HandleApply();
};
