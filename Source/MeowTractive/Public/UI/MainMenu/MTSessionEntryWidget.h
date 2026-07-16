#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "MTSessionEntryWidget.generated.h"

class UMTSessionData;
class UCommonTextBlock;

/** 서버 브라우저 목록의 한 줄. 이름/인원 표시는 C++, Join은 Flow에 위임. */
UCLASS()
class MEOWTRACTIVE_API UMTSessionEntryWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	// 행 UI 추가 갱신 — BP가 선택적으로 구현 (이름/인원은 C++이 채움)
	UFUNCTION(BlueprintImplementableEvent, Category = "MT|Browser")
	void OnSessionDataSet(UMTSessionData* Data);

	// Join 버튼 → 이 세션 참가
	UFUNCTION(BlueprintCallable, Category = "MT|Browser")
	void JoinThisSession();

protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> ServerText;

	// 인원("3/4") 또는 비공개 방이면 "?"
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> StateText;

	UPROPERTY(BlueprintReadOnly, Category = "MT|Browser")
	TObjectPtr<UMTSessionData> SessionData;
};
