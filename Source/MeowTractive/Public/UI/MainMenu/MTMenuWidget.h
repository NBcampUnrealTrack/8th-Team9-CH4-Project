#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MTMenuWidget.generated.h"

class UMTGameInstance;

/** 메인메뉴 위젯: 표현 + 의도 전달만. 세션/트래블 로직은 UMTGameInstance(Flow)가 담당. */
UCLASS()
class MEOWTRACTIVE_API UMTMenuWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MT|Menu")
	void MenuSetup(int32 NumPublicConnections = 4, bool bIsLAN = false);

	// 버튼 OnClicked → Flow에 의도 전달
	UFUNCTION(BlueprintCallable, Category = "MT|Menu")
	void HostButtonClicked();

	UFUNCTION(BlueprintCallable, Category = "MT|Menu")
	void JoinButtonClicked();

protected:
	// CommonUI: 입력 모드/커서를 활성화 트리로 관리 (raw SetInputMode 대신)
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

private:
	UPROPERTY()
	TObjectPtr<UMTGameInstance> GameFlow;

	int32 NumConnections = 4;
	bool bUseLAN = false;
};
