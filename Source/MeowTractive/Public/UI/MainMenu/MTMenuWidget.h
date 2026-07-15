#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Game/MTTypes.h"
#include "MTMenuWidget.generated.h"

class UMTGameInstance;
class UMTSettingsWidget;
class UCommonButtonBase;
class UWidget;
class UTextBlock;

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

	// 설정 화면 열기 ('설정' 버튼 → 일시정지 메뉴와 동일한 WBP_MTSettings)
	UFUNCTION(BlueprintCallable, Category = "MT|Menu")
	void OpenSettings();

protected:
	virtual void NativeConstruct() override;

	// 메인 화면 '설정' 버튼 (WBP 이름 그대로 자동 바인딩 — 현재 BP 미배선이라 C++이 처리)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> Mt_MainSetting;

	// 설정 화면 위젯 (WBP_MTSettings)
	UPROPERTY(EditDefaultsOnly, Category = "MT|Menu")
	TSubclassOf<UMTSettingsWidget> SettingsWidgetClass;

	// CommonUI: 입력 모드/커서를 활성화 트리로 관리 (raw SetInputMode 대신)
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	virtual void NativeDestruct() override;

	// 접속 진행 중 전체 클릭 차단 + "접속 중" 표시 (WBP의 같은 이름 위젯에 자동 바인딩)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ConnectingOverlay;

	// "접속 중" 문구 — 표시 중 점(. → .. → ...) 애니메이션 대상
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ConnectingText;

private:
	UFUNCTION()
	void HandleConnectingStateChanged(bool bConnecting);

	// 점 개수 순환 갱신 (타이머)
	void TickConnectingDots();

	FTimerHandle ConnectingDotsTimer;
	int32 ConnectingDotCount = 0;
	FString ConnectingBaseText;   // WBP 원문에서 끝 점을 뗀 기본 문구

	// 세션 생성/검색에 쓸 LAN 여부 — 콘솔 `MT.Local 1`이 MenuSetup 값보다 우선
	bool UseLANForSession() const;

	UPROPERTY()
	TObjectPtr<UMTGameInstance> GameFlow;

	int32 NumConnections = 4;
	bool bUseLAN = false;
};
