#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "MTPlayerHUD.generated.h"

class UMTPlayerWidget;

/** 인게임 플레이어 HUD — 화면 위젯 생성/소유를 담당 (HUD는 로컬 클라이언트에만 스폰됨). */
UCLASS()
class MEOWTRACTIVE_API AMTPlayerHUD : public AHUD
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "MT|UI")
	TSubclassOf<UMTPlayerWidget> PlayerWidgetClass;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<UMTPlayerWidget> PlayerWidget;
};
