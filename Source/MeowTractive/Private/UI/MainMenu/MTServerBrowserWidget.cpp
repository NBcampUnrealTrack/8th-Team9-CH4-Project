#include "UI/MainMenu/MTServerBrowserWidget.h"
#include "Game/MTGameInstance.h"
#include "Online/MTSessionData.h"
#include "Components/ListView.h"

void UMTServerBrowserWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	GameFlow = Cast<UMTGameInstance>(GetGameInstance());
	if (GameFlow)
	{
		GameFlow->OnSessionsFound.AddDynamic(this, &UMTServerBrowserWidget::HandleSessionsFound);
	}
	RefreshSessions();
}

void UMTServerBrowserWidget::NativeOnDeactivated()
{
	if (GameFlow)
	{
		GameFlow->OnSessionsFound.RemoveDynamic(this, &UMTServerBrowserWidget::HandleSessionsFound);
	}
	Super::NativeOnDeactivated();
}

void UMTServerBrowserWidget::RefreshSessions()
{
	if (GameFlow)
	{
		GameFlow->JoinGame(bUseLAN);   // FindSessions → 결과는 OnSessionsFound로
	}
}

void UMTServerBrowserWidget::HandleSessionsFound(const TArray<UMTSessionData*>& Sessions)
{
	if (!SessionList)
	{
		return;
	}
	SessionList->ClearListItems();
	for (UMTSessionData* Data : Sessions)
	{
		SessionList->AddItem(Data);
	}
}

TOptional<FUIInputConfig> UMTServerBrowserWidget::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}
