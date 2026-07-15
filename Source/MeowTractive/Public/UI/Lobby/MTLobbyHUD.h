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

private:
	// 내 폰 준비 → 페이드인 공개
	void TryReveal();

	float ShownTime = 0.f;
	FTimerHandle RevealTimer;
};
