#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "MTLoadingOverlayHUD.generated.h"

class UUserWidget;
class SWidget;
class STextBlock;

/** 로딩 오버레이 공통 HUD 베이스 — 표시/제거/페이드만 제공, 시점·해제 조건은 서브클래스가 결정. */
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

	// 오버레이를 서서히 걷어 게임 화면 페이드인 (Duration 후 제거)
	void FadeOutLoadingOverlay(float Duration = 1.f);

	// 폴백 오버레이 문구 갱신 (점 애니메이션용 — WBP 지정 시 무시, WBP가 자체 연출)
	void SetFallbackOverlayText(const FText& NewText);

	// 폴백 오버레이 문구 (서브클래스 생성자/BP에서 교체)
	UPROPERTY(EditDefaultsOnly, Category = "MT|UI")
	FText FallbackLoadingText = NSLOCTEXT("MT", "Loading", "로딩 중...");

private:
	// 페이드 진행 (타이머)
	void TickFade();

	UPROPERTY()
	TObjectPtr<UUserWidget> LoadingWidget;

	// LoadingWidgetClass 미지정 시 폴백 (Slate 직접 구성)
	TSharedPtr<SWidget> LoadingSlateWidget;
	TSharedPtr<STextBlock> FallbackTextBlock;

	FTimerHandle FadeTimer;
	float FadeElapsed = 0.f;
	float FadeDuration = 0.f;
};
