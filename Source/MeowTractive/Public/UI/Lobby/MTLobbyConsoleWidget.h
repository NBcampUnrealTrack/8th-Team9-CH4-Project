#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MTLobbyConsoleWidget.generated.h"

class UCommonButtonBase;

/** 로비 호스트 콘솔 위젯 공통 베이스: 닫기 버튼/ESC → 제거 + 커서 복원. */
UCLASS(Abstract)
class MEOWTRACTIVE_API UMTLobbyConsoleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 닫기 + 커서/입력모드 복원
	UFUNCTION(BlueprintCallable, Category = "MT|Lobby")
	void CloseConsole();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> CloseButton;

private:
	void HandleClose();
};
