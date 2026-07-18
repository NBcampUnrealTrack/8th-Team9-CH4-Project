#include "UI/Lobby/MTLobbyConsoleWidget.h"
#include "CommonButtonBase.h"
#include "GameFramework/PlayerController.h"

void UMTLobbyConsoleWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);
	SetUserFocus(GetOwningPlayer());   // ESC 수신용

	if (CloseButton)
	{
		CloseButton->OnClicked().AddUObject(this, &UMTLobbyConsoleWidget::HandleClose);
	}
}

void UMTLobbyConsoleWidget::NativeDestruct()
{
	// CommonButtonBase는 non-dynamic 이벤트 → 수동 해제
	if (CloseButton)
	{
		CloseButton->OnClicked().RemoveAll(this);
	}
	Super::NativeDestruct();
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
