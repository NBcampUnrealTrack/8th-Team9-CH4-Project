#include "UI/MainMenu/MTSessionEntryWidget.h"
#include "Online/MTSessionData.h"
#include "Game/MTGameInstance.h"
#include "CommonTextBlock.h"

void UMTSessionEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	SessionData = Cast<UMTSessionData>(ListItemObject);

	// ListView가 행 위젯을 재사용하므로 매번 두 텍스트를 모두 덮어씀
	if (SessionData)
	{
		if (ServerText)
		{
			ServerText->SetText(FText::FromString(SessionData->ServerName));
		}
		if (StateText)
		{
			// 비공개 방은 인원을 감추고 "?" 표시
			StateText->SetText(SessionData->bHasPassword
				? FText::FromString(TEXT("?"))
				: FText::FromString(FString::Printf(TEXT("%d/%d"), SessionData->CurrentPlayers, SessionData->MaxSlots)));
		}
	}

	OnSessionDataSet(SessionData);
}

void UMTSessionEntryWidget::JoinThisSession()
{
	if (UMTGameInstance* GI = Cast<UMTGameInstance>(GetGameInstance()))
	{
		GI->JoinSessionData(SessionData);
	}
}
