#pragma once

#include "CoreMinimal.h"
#include "UI/MTLoadingOverlayHUD.h"
#include "MTPlayerHUD.generated.h"

class UMTPlayerWidget;
class STextBlock;

/** 인게임 플레이어 HUD — 화면 위젯 생성/소유 + 대기 오버레이(점 애니메이션) → 3·2·1·시작! 카운트다운 연출. */
UCLASS()
class MEOWTRACTIVE_API AMTPlayerHUD : public AMTLoadingOverlayHUD
{
	GENERATED_BODY()

public:
	AMTPlayerHUD();

	UPROPERTY(EditDefaultsOnly, Category = "MT|UI")
	TSubclassOf<UMTPlayerWidget> PlayerWidgetClass;

	// 카운트다운 시작 시 오버레이 페이드아웃 시간(초) — 게임 화면 페이드인
	UPROPERTY(EditDefaultsOnly, Category = "MT|UI", meta = (ClampMin = "0.0"))
	float RevealFadeDuration = 1.f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 클라는 GameState가 HUD보다 늦게 올 수 있음 → 준비될 때까지 재시도
	void TryBindGameState();

	UFUNCTION()
	void HandleMatchStateChanged();

	// 3→2→1→0(시작!) 표시 + 첫 값에서 오버레이 페이드아웃
	UFUNCTION()
	void HandleStartCountdownChanged(int32 Value);

	// 대기 문구 점 애니메이션 (. → .. → ... 반복)
	void TickWaitingDots();

	// 화면 중앙 카운트다운 텍스트 (Slate)
	void ShowCountdownText(const FText& Text);
	void RemoveCountdownText();

	UPROPERTY()
	TObjectPtr<UMTPlayerWidget> PlayerWidget;

	TSharedPtr<SWidget> CountdownContainer;
	TSharedPtr<STextBlock> CountdownTextBlock;

	FTimerHandle BindRetryTimer;
	FTimerHandle WaitingDotsTimer;
	FTimerHandle CountdownHideTimer;
	int32 WaitingDotCount = 0;
	FString WaitingBaseText;      // FallbackLoadingText에서 끝 점을 뗀 기본 문구
	bool bCountdownShown = false;
};
