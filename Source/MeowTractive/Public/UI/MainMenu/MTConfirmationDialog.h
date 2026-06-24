#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MTConfirmationDialog.generated.h"

class UWidgetAnimation;

// 확인(네) / 취소(아니요) 결과 — 여는 쪽이 바인딩
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMTOnDialogConfirmed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMTOnDialogCancelled);

/** 범용 확인 다이얼로그: 열릴 때 FadeIn 재생, 네=Confirm 방송, 아니요=Cancel 후 닫기.
 *  "네를 누르면 무엇을 하는지"는 다이얼로그가 모름 — 여는 쪽이 OnConfirmed에 연결. */
UCLASS()
class MEOWTRACTIVE_API UMTConfirmationDialog : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// "네" 버튼 OnClicked
	UFUNCTION(BlueprintCallable, Category = "MT|Dialog")
	void Confirm();

	// "아니요" 버튼 OnClicked
	UFUNCTION(BlueprintCallable, Category = "MT|Dialog")
	void Cancel();

	UPROPERTY(BlueprintAssignable, Category = "MT|Dialog")
	FMTOnDialogConfirmed OnConfirmed;

	UPROPERTY(BlueprintAssignable, Category = "MT|Dialog")
	FMTOnDialogCancelled OnCancelled;

protected:
	virtual void NativeConstruct() override;

	// WBP 애니메이션 이름을 "FadeIn"으로 두면 자동 바인딩
	UPROPERTY(meta = (BindWidgetAnimOptional), Transient)
	TObjectPtr<UWidgetAnimation> FadeIn;
};
