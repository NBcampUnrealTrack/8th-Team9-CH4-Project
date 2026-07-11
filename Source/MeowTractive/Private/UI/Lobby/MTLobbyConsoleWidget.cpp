#include "UI/Lobby/MTLobbyConsoleWidget.h"
#include "Components/Button.h"
#include "GameFramework/PlayerController.h"

void UMTLobbyConsoleWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);
	SetUserFocus(GetOwningPlayer());   // ESC 수신용

	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &UMTLobbyConsoleWidget::HandleClose);
	}
}

FReply UMTLobbyConsoleWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		CloseConsole();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UMTLobbyConsoleWidget::HandleClose()
{
	CloseConsole();
}

void UMTLobbyConsoleWidget::CloseConsole()
{
	// UIOnly 해제 → 게임 입력 복원 (열기는 AMTLobbyHostConsole이 수행)
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	RemoveFromParent();
}
