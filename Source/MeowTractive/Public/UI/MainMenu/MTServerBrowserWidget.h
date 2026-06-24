#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MTServerBrowserWidget.generated.h"

class UListView;
class UMTGameInstance;
class UMTSessionData;

/** 서버 브라우저: 세션 검색 → ListView 표시. 참가/트래블은 Flow가 담당. */
UCLASS()
class MEOWTRACTIVE_API UMTServerBrowserWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// 새로고침 버튼 → 세션 재검색
	UFUNCTION(BlueprintCallable, Category = "MT|Browser")
	void RefreshSessions();

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	UFUNCTION()
	void HandleSessionsFound(const TArray<UMTSessionData*>& Sessions);

	// WBP에 이름 "SessionList"인 ListView
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UListView> SessionList;

	// 원격 Steam 테스트=인터넷(LAN 아님). 추후 옵션 노출.
	UPROPERTY(EditAnywhere, Category = "MT|Browser")
	bool bUseLAN = false;

private:
	UPROPERTY()
	TObjectPtr<UMTGameInstance> GameFlow;
};
