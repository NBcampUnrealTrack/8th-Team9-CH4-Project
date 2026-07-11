#include "UI/Lobby/MTLobbyKickWidget.h"
#include "Player/MTPlayerController.h"
#include "Player/MTPlayerState.h"
#include "Components/ListView.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UMTKickEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (KickButton)
	{
		KickButton->OnClicked.AddDynamic(this, &UMTKickEntryWidget::HandleKick);
	}
}

void UMTKickEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	TargetPlayer = Cast<APlayerState>(ListItemObject);
	if (NameText)
	{
		NameText->SetText(FText::FromString(
			TargetPlayer.IsValid() ? TargetPlayer->GetPlayerName() : TEXT("???")));
	}
}

void UMTKickEntryWidget::HandleKick()
{
	AMTPlayerController* PC = Cast<AMTPlayerController>(GetOwningPlayer());
	if (PC && TargetPlayer.IsValid())
	{
		PC->Server_KickPlayer(TargetPlayer.Get());   // GameMode가 호스트 검증
	}
}

void UMTLobbyKickWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RefreshList();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RefreshTimer, this, &UMTLobbyKickWidget::RefreshList, 1.f, true);
	}
}

void UMTLobbyKickWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}
	Super::NativeDestruct();
}

void UMTLobbyKickWidget::RefreshList()
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!PlayerList || !GS)
	{
		return;
	}

	// 호스트(나) 제외 접속자
	TArray<UObject*> Items;
	for (APlayerState* PS : GS->PlayerArray)
	{
		const AMTPlayerState* MTPS = Cast<AMTPlayerState>(PS);
		if (MTPS && !MTPS->IsHost())
		{
			Items.Add(PS);
		}
	}

	// 인원 변동 없으면 유지 (선택/스크롤 churn 방지)
	if (Items.Num() == PlayerList->GetNumItems())
	{
		return;
	}
	PlayerList->SetListItems(Items);
}
