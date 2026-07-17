#pragma once

#include "CoreMinimal.h"
#include "UI/MTLoadingOverlayHUD.h"
#include "MTLobbyHUD.generated.h"

/** 로비 HUD — 로비 도착(최초 입장·매치 복귀) 직후를 검은 화면으로 덮고 페이드인 (문구 없음). */
UCLASS()
class MEOWTRACTIVE_API AMTLobbyHUD : public AMTLoadingOverlayHUD
{
	GENERATED_BODY()

public:
	AMTLobbyHUD();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 페이드인 시간(초) — 내 폰 준비 즉시 이 시간 동안 서서히 공개
	UPROPERTY(EditDefaultsOnly, Category = "MT|UI", meta = (ClampMin = "0.0"))
	float RevealFadeDuration = 1.5f;

	// 폰 준비가 늦어도 이 시간이 지나면 강제 공개 (오버레이 고착 방지)
	UPROPERTY(EditDefaultsOnly, Category = "MT|UI", meta = (ClampMin = "0.0"))
	float MaxDisplayTime = 5.f;

	// 카운트다운 0("게임시작!") 후 페이드아웃 시작까지 대기(초) — 문구 보여주는 시간
	UPROPERTY(EditDefaultsOnly, Category = "MT|UI", meta = (ClampMin = "0.0"))
	float StartFadeDelay = 0.5f;

	// 트래블 직전 검은 화면 페이드아웃 시간(초). GameMode StartTravelDelay와 합이 맞아야 함
	UPROPERTY(EditDefaultsOnly, Category = "MT|UI", meta = (ClampMin = "0.0"))
	float TravelFadeDuration = 1.f;

private:
	// 내 폰 준비 → 페이드인 공개
	void TryReveal();

	// 로비 GameState 카운트다운 구독 (복제 지연 대비 재시도)
	void TryBindCountdown();

	UFUNCTION()
	void HandleStartCountdownChanged(int32 RemainingSeconds);

	// "게임시작!" 노출 후 검은 화면으로 페이드아웃
	void StartTravelFade();

	float ShownTime = 0.f;
	FTimerHandle RevealTimer;
	FTimerHandle CountdownBindTimer;
	FTimerHandle TravelFadeTimer;
	bool bTravelFadeStarted = false;
};
