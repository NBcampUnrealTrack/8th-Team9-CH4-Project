#pragma once

#include "CoreMinimal.h"
#include "UI/Lobby/MTLobbyConsoleWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "MTLobbyKickWidget.generated.h"

class UListView;
class UTextBlock;
class UButton;
class APlayerState;

/** 강퇴 목록 한 줄: 닉네임 + 강퇴 버튼. 리스트 아이템 = APlayerState. */
UCLASS()
class MEOWTRACTIVE_API UMTKickEntryWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> KickButton;

private:
	UFUNCTION()
	void HandleKick();

	TWeakObjectPtr<APlayerState> TargetPlayer;
};

/** 호스트 콘솔 — 강퇴: 호스트 제외 접속자 목록 + 강퇴 버튼. */
UCLASS()
class MEOWTRACTIVE_API UMTLobbyKickWidget : public UMTLobbyConsoleWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UListView> PlayerList;

private:
	// 접속자 변동 대비 주기 갱신
	void RefreshList();

	FTimerHandle RefreshTimer;
};
