#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Game/MTTypes.h"
#include "MTMenuWidget.generated.h"

class UMTGameInstance;

/** 메인메뉴 위젯: 표현 + 의도 전달만. 세션/트래블 로직은 UMTGameInstance(Flow)가 담당. */
UCLASS()
class MEOWTRACTIVE_API UMTMenuWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MT|Menu")
	void MenuSetup(int32 NumPublicConnections = 4, bool bIsLAN = false);

	// 빠른 시작: 참가 가능한 공개방 있으면 참가, 없으면 기본 설정 방 생성
	UFUNCTION(BlueprintCallable, Category = "MT|Menu")
	void HostButtonClicked();

	// 방생성 창의 설정으로 호스트 (Conn/LAN은 MenuSetup 값 사용)
	UFUNCTION(BlueprintCallable, Category = "MT|Menu")
	void HostWithSettings(FMTRoomSettings RoomSettings);

	UFUNCTION(BlueprintCallable, Category = "MT|Menu")
	void JoinButtonClicked();

protected:
	// CommonUI: 입력 모드/커서를 활성화 트리로 관리 (raw SetInputMode 대신)
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

private:
	UPROPERTY()
	TObjectPtr<UMTGameInstance> GameFlow;

	int32 NumConnections = 4;
	bool bUseLAN = true;
};
