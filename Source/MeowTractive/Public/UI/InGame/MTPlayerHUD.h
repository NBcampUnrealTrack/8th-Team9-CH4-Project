#pragma once

#include "CoreMinimal.h"
#include "UI/MTLoadingOverlayHUD.h"
#include "MTPlayerHUD.generated.h"

class UMTPlayerWidget;

/** 인게임 플레이어 HUD — 화면 위젯 생성/소유 + StartMatch까지 로딩 오버레이 유지. */
UCLASS()
class MEOWTRACTIVE_API AMTPlayerHUD : public AMTLoadingOverlayHUD
{
	GENERATED_BODY()

public:
	AMTPlayerHUD();

	UPROPERTY(EditDefaultsOnly, Category = "MT|UI")
	TSubclassOf<UMTPlayerWidget> PlayerWidgetClass;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 클라는 GameState가 HUD보다 늦게 올 수 있음 → 준비될 때까지 재시도
	void TryBindMatchState();

	UFUNCTION()
	void HandleMatchStateChanged();

	UPROPERTY()
	TObjectPtr<UMTPlayerWidget> PlayerWidget;

	FTimerHandle BindRetryTimer;
};
