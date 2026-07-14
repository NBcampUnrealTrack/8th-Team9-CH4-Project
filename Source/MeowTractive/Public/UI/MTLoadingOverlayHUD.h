#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "MTLoadingOverlayHUD.generated.h"

class UUserWidget;
class SWidget;

/** 로딩 오버레이 공통 HUD 베이스 — 표시/제거만 제공, 표시 시점·해제 조건은 서브클래스가 결정. */
UCLASS(Abstract)
class MEOWTRACTIVE_API AMTLoadingOverlayHUD : public AHUD
{
	GENERATED_BODY()

public:
	// 오버레이 WBP (미지정 시 기본 검은 화면 + FallbackLoadingText)
	UPROPERTY(EditDefaultsOnly, Category = "MT|UI")
	TSubclassOf<UUserWidget> LoadingWidgetClass;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void ShowLoadingOverlay();
	void RemoveLoadingOverlay();

	// 폴백 오버레이 문구 (서브클래스 생성자/BP에서 교체)
	UPROPERTY(EditDefaultsOnly, Category = "MT|UI")
	FText FallbackLoadingText = NSLOCTEXT("MT", "Loading", "로딩 중...");

private:
	UPROPERTY()
	TObjectPtr<UUserWidget> LoadingWidget;

	// LoadingWidgetClass 미지정 시 폴백 (Slate 직접 구성)
	TSharedPtr<SWidget> LoadingSlateWidget;
};
