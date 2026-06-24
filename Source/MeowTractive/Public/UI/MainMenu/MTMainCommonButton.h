#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "MTMainCommonButton.generated.h"

class UTextBlock;

/** 메인메뉴 버튼: 텍스트만 담당. 시각 상태(투명/호버 흰색)는 CommonButtonStyle 에셋에서. */
UCLASS()
class MEOWTRACTIVE_API UMTMainCommonButton : public UCommonButtonBase
{
	GENERATED_BODY()

protected:
	virtual void NativePreConstruct() override;

	// WBP의 텍스트 위젯 이름 "Label" 바인딩
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Label;

	// 버튼 텍스트 변수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MT|Button")
	FText ButtonText = FText::FromString("Button");
};
