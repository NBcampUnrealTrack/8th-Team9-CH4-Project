#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "MTSessionEntryWidget.generated.h"

class UMTSessionData;

/** 서버 브라우저 목록의 한 줄. 데이터 표시는 BP, Join은 Flow에 위임. */
UCLASS()
class MEOWTRACTIVE_API UMTSessionEntryWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	// 행 UI(이름/핑/인원) 갱신 — BP가 구현
	UFUNCTION(BlueprintImplementableEvent, Category = "MT|Browser")
	void OnSessionDataSet(UMTSessionData* Data);

	// Join 버튼 → 이 세션 참가
	UFUNCTION(BlueprintCallable, Category = "MT|Browser")
	void JoinThisSession();

protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	UPROPERTY(BlueprintReadOnly, Category = "MT|Browser")
	TObjectPtr<UMTSessionData> SessionData;
};
