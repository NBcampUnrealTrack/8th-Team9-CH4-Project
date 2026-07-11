#include "UI/PauseMenu/MTPauseMenuWidget.h"
#include "Player/MTPlayerController.h"
#include "Game/MTGameInstance.h"
#include "UI/Settings/MTSettingsWidget.h"
#include "Components/Button.h"

void UMTPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);   // ESC 키 수신용

	if (ResumeButton)
	{
		ResumeButton->OnClicked.AddDynamic(this, &UMTPauseMenuWidget::HandleResume);
	}
	if (LeaveButton)
	{
		LeaveButton->OnClicked.AddDynamic(this, &UMTPauseMenuWidget::HandleLeave);
	}
	if (SettingsButton)
	{
		SettingsButton->OnClicked.AddDynamic(this, &UMTPauseMenuWidget::HandleSettings);
	}
}

void UMTPauseMenuWidget::HandleSettings()
{
	if (!SettingsWidgetClass)
	{
		return;
	}
	if (UMTSettingsWidget* Settings = CreateWidget<UMTSettingsWidget>(GetOwningPlayer(), SettingsWidgetClass))
	{
		Settings->AddToViewport(60);   // 일시정지 메뉴(50) 위
		Settings->SetUserFocus(GetOwningPlayer());   // ESC 닫기 수신
	}
}

FReply UMTPauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		CloseMenu();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UMTPauseMenuWidget::HandleResume()
{
	CloseMenu();
}

void UMTPauseMenuWidget::CloseMenu()
{
	if (AMTPlayerController* PC = Cast<AMTPlayerController>(GetOwningPlayer()))
	{
		PC->TogglePauseMenu();   // 열려 있으므로 닫힘 처리
	}
}

void UMTPauseMenuWidget::HandleLeave()
{
	// 나가기 = Flow(GameInstance)가 세션 정리 + 메인메뉴 복귀
	if (UMTGameInstance* GI = Cast<UMTGameInstance>(GetGameInstance()))
	{
		GI->LeaveGame();
	}
}
