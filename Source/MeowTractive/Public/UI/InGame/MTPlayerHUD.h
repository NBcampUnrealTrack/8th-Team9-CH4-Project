#pragma once

#include "CoreMinimal.h"
#include "UI/MTLoadingOverlayHUD.h"
#include "Components/SlateWrapperTypes.h"
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

	// 3·2·1 표시 순간의 틱 사운드 (표시 타이밍과 동기)
	UPROPERTY(EditDefaultsOnly, Category = "MT|UI")
	TObjectPtr<USoundBase> CountdownTickSound;

	// "시작!" 표시 순간의 사운드
	UPROPERTY(EditDefaultsOnly, Category = "MT|UI")
	TObjectPtr<USoundBase> MatchStartSound;

	UMTPlayerWidget* GetPlayerWidget() const { return PlayerWidget; }

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

	// 페이드인 완료 후 카운트 표시 시작 (페이드 중엔 값만 저장)
	void HandleRevealFinished();

	// 카운트다운 동안 숨긴 HUD 위젯을 "시작!" 이후 표시
	void ShowPlayerWidget();

	// 현재 카운트 값 표시 (>0 숫자, 0 = 시작!)
	void UpdateCountdownText(int32 Value);

	FTimerHandle BindRetryTimer;
	FTimerHandle WaitingDotsTimer;
	FTimerHandle CountdownHideTimer;
	FTimerHandle RevealDoneTimer;
	int32 WaitingDotCount = 0;
	FString WaitingBaseText;      // FallbackLoadingText에서 끝 점을 뗀 기본 문구
	bool bCountdownShown = false;
	bool bRevealFinished = false;         // 페이드인 완료 여부
	int32 PendingCountdownValue = -1;     // 페이드 중 도착한 최신 카운트 값
	bool bPlayerWidgetRevealed = false;   // HUD 위젯 표시 완료 여부
	ESlateVisibility SavedPlayerWidgetVisibility = ESlateVisibility::SelfHitTestInvisible;   // 숨기기 전 원래 가시성
};
