#include "UI/Lobby/MTLobbyRoomSettingsWidget.h"
#include "Game/MTGameInstance.h"
#include "Player/MTPlayerController.h"
#include "Components/EditableTextBox.h"
#include "Components/ComboBoxString.h"
#include "Components/Button.h"

void UMTLobbyRoomSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ApplyButton)
	{
		ApplyButton->OnClicked.AddDynamic(this, &UMTLobbyRoomSettingsWidget::HandleApply);
	}

	// 현재 방 설정으로 초기화 (호스트 전용 UI — GameInstance 값이 권위)
	const UMTGameInstance* GI = Cast<UMTGameInstance>(GetGameInstance());
	const FMTRoomSettings Current = GI ? GI->GetRoomSettings() : FMTRoomSettings();

	if (RoomNameInput)
	{
		RoomNameInput->SetText(FText::FromString(Current.RoomName));
	}
	if (PasswordInput)
	{
		PasswordInput->SetText(FText::FromString(Current.Password));
	}
	if (GameModeCombo)
	{
		GameModeCombo->ClearOptions();
		GameModeCombo->AddOption(TEXT("개인전"));
		GameModeCombo->AddOption(TEXT("팀전"));
		GameModeCombo->AddOption(TEXT("고양이 전투"));
		GameModeCombo->SetSelectedIndex((int32)Current.GameMode);
	}
	if (MapCombo)
	{
		MapCombo->ClearOptions();
		MapCombo->AddOption(TEXT("인사동"));
		MapCombo->AddOption(TEXT("랜덤"));
		MapCombo->SetSelectedIndex((int32)Current.Map);
	}
}

void UMTLobbyRoomSettingsWidget::HandleApply()
{
	AMTPlayerController* PC = Cast<AMTPlayerController>(GetOwningPlayer());
	if (!PC)
	{
		return;
	}

	FMTRoomSettings NewSettings;
	if (RoomNameInput)
	{
		NewSettings.RoomName = RoomNameInput->GetText().ToString();
	}
	if (PasswordInput)
	{
		NewSettings.Password = PasswordInput->GetText().ToString();
	}
	if (GameModeCombo)
	{
		NewSettings.GameMode = (EMTRoomGameMode)FMath::Max(0, GameModeCombo->GetSelectedIndex());
	}
	if (MapCombo)
	{
		NewSettings.Map = (EMTRoomMap)FMath::Max(0, MapCombo->GetSelectedIndex());
	}

	PC->Server_UpdateRoomSettings(NewSettings);   // GameMode가 호스트 검증
	CloseConsole();
}
