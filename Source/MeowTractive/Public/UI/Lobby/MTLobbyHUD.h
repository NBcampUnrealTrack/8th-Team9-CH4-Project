#pragma once

#include "CoreMinimal.h"
#include "UI/MTLoadingOverlayHUD.h"
#include "MTLobbyHUD.generated.h"

/** 로비 HUD — 로비 도착(최초 입장·매치 복귀 seamless) 직후 스트리밍 pop-in을 로딩 오버레이로 가림. */
UCLASS()
class MEOWTRACTIVE_API AMTLobbyHUD : public AMTLoadingOverlayHUD
{
	GENERATED_BODY()

public:
	AMTLobbyHUD();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 오버레이 최소 표시 시간(초) — 도착 직후 텍스처/메시 스트리밍 가림용
	UPROPERTY(EditDefaultsOnly, Category = "MT|UI", meta = (ClampMin = "0.0"))
	float MinDisplayTime = 1.5f;

	// 폰 준비가 늦어도 이 시간이 지나면 강제 공개 (오버레이 고착 방지)
	UPROPERTY(EditDefaultsOnly, Category = "MT|UI", meta = (ClampMin = "0.0"))
	float MaxDisplayTime = 5.f;

private:
	// 내 폰 준비 + 최소 시간 경과 → 공개
	void TryReveal();

	float ShownTime = 0.f;
	FTimerHandle RevealTimer;
};
