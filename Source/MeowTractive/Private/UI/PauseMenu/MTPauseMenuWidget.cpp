#include "UI/PauseMenu/MTPauseMenuWidget.h"
#include "Player/MTPlayerController.h"
#include "Game/MTGameInstance.h"
#include "UI/Settings/MTSettingsWidget.h"
#include "CommonButtonBase.h"

void UMTPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);   // ESC 키 수신용

	if (ResumeButton)
	{
		ResumeButton->OnClicked().AddUObject(this, &UMTPauseMenuWidget::HandleResume);
	}
	if (LeaveButton)
	{
		LeaveButton->OnClicked().AddUObject(this, &UMTPauseMenuWidget::HandleLeave);
	}
	if (SettingsButton)
	{
		SettingsButton->OnClicked().AddUObject(this, &UMTPauseMenuWidget::HandleSettings);
	}
}

void UMTPauseMenuWidget::NativeDestruct()
{
	// CommonButtonBase는 non-dynamic 이벤트 → 수동 해제
	if (ResumeButton)
	{
		ResumeButton->OnClicked().RemoveAll(this);
	}
	if (LeaveButton)
	{
		LeaveButton->OnClicked().RemoveAll(this);
	}
	if (SettingsButton)
	{
		SettingsButton->OnClicked().RemoveAll(this);
	}
	Super::NativeDestruct();
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

		// 설정이 떠 있는 동안 일시정지 메뉴 숨김 → 닫히면 복원 (+ ESC 수신 위해 포커스 반환)
		SetVisibility(ESlateVisibility::Collapsed);
		Settings->OnClosed.AddWeakLambda(this, [this]()
		{
			SetVisibility(ESlateVisibility::Visible);
			SetUserFocus(GetOwningPlayer());
		});
	}
}

FReply UMTPauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// UIOnly 동안 IA_Pause가 못 오므로 위젯이 직접 닫기 (ESC + PIE용 P — IMC 매핑과 동일 키)
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::P)
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
