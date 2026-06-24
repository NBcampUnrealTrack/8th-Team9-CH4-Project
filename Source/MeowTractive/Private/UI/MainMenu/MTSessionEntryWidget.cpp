#include "UI/MainMenu/MTSessionEntryWidget.h"
#include "Online/MTSessionData.h"
#include "Game/MTGameInstance.h"

void UMTSessionEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	SessionData = Cast<UMTSessionData>(ListItemObject);
	OnSessionDataSet(SessionData);   // BP가 텍스트 채움
}

void UMTSessionEntryWidget::JoinThisSession()
{
	if (UMTGameInstance* GI = Cast<UMTGameInstance>(GetGameInstance()))
	{
		GI->JoinSessionData(SessionData);
	}
}
