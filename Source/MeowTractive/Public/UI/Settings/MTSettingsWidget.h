#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/InputKeySelector.h"
#include "Engine/TimerHandle.h"
#include "MTSettingsWidget.generated.h"

class UMTMainCommonButton;
class USlider;
class UProgressBar;
class UComboBoxString;
class UCheckBox;
class UVerticalBox;
class UCommonAnimatedSwitcher;
class UCommonButtonBase;
class UEnhancedInputUserSettings;
class UInputMappingContext;
class UTextBlock;

// 키 셀렉터 → 행 위젯으로 결과 전달 (어느 매핑인지 포함)
DECLARE_DELEGATE_TwoParams(FMTOnKeyRebind, FName /*MappingName*/, FKey /*NewKey*/);

// 행 위젯 → 설정 위젯: 리바인드 요청. 적용되면 true, 다른 액션과 중복이면 false (행이 원래 키로 복귀)
DECLARE_DELEGATE_RetVal_TwoParams(bool, FMTOnKeyRebindRequest, FName /*MappingName*/, FKey /*NewKey*/);

/** 재바인딩용 키 셀렉터: 자기 매핑 이름을 알고 선택 결과를 넘긴다 (C++ 동적 생성 전용). */
UCLASS()
class MEOWTRACTIVE_API UMTKeySelector : public UInputKeySelector
{
	GENERATED_BODY()

public:
	void Init(FName InMappingName);

	FMTOnKeyRebind OnKeyRebind;

private:
	// OnKeySelected 구독 핸들러 — 유효 키만 OnKeyRebind로 전달
	// (베이스 HandleKeySelected는 private virtual이라 오버라이드 대신 델리게이트 구독)
	UFUNCTION()
	void HandleChordSelected(FInputChord Chord);

	FName MappingName;
};

/** 키 재바인딩 한 행 (WBP에서 레이아웃/스타일 구성, 이 클래스는 행별 데이터만 채움). */
UCLASS()
class MEOWTRACTIVE_API UMTKeyRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(FName InMappingName, const FText& InLabelText, const FKey& InCurrentKey);

	FMTOnKeyRebindRequest OnKeyRebind;

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Label;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UMTKeySelector> Selector;

private:
	UFUNCTION()
	void HandleSelectorRebind(FName MappingName, FKey NewKey);

	// 현재 적용된 키 (거부 시 이 값으로 셀렉터를 되돌림)
	FKey CurrentKey;
};

/** 설정 화면: 볼륨(마스터/배경음/효과음) + 그래픽(창모드/해상도/품질/수직동기) + 키 재바인딩.
 *  일시정지 메뉴·메인메뉴 공용. 볼륨은 즉시 적용, 그래픽은 적용 버튼, 키는 선택 즉시 저장. */
UCLASS()
class MEOWTRACTIVE_API UMTSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 설정 창이 닫힐 때(버튼/ESC 공통) 방송 — 여는 쪽이 자기 UI 복원에 사용
	FSimpleMulticastDelegate OnClosed;

protected:
	virtual void NativeConstruct() override;

	// ESC → 닫기 (일시정지 메뉴가 UIOnly라 위젯이 직접 처리)
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	virtual void NativeDestruct() override;

	// --- 탭 (볼륨/그래픽/키) ---
	// 페이지 스위처 — 자식 0=오디오, 1=그래픽, 2=키 순서 (없으면 탭 없이 기존 단일 레이아웃 유지)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonAnimatedSwitcher> TabSwitcher;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> AudioTabButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> GraphicsTabButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> KeyTabButton;

	// --- 오디오 (0~1) ---
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> MasterSlider;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> BGMSlider;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> SFXSlider;

	// 슬라이더 뒤에 겹쳐 깔린 채움 바 — 슬라이더 값과 같이 갱신 (Slider는 조작, ProgressBar는 표시 담당)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> PB_MasterSlider;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> PB_BGMSlider;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> PB_SFXSlider;

	// --- 그래픽 ---
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> WindowModeCombo;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> ResolutionCombo;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> QualityCombo;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> VSyncCheck;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> ApplyGraphicsButton;

	// --- 키 재바인딩 (행 위젯(WBP_MTKeyRow)을 C++이 KeyListBox에 채워넣음) ---
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> KeyListBox;

	// 키 행 위젯 클래스 (레이아웃/스타일은 이 WBP에서, 행별 텍스트만 코드에서 채움)
	UPROPERTY(EditDefaultsOnly, Category = "MT|UI")
	TSubclassOf<UMTKeyRowWidget> KeyRowWidgetClass;

	// 키 중복 시 잠깐 표시되는 경고 문구 (WBP에 두면 표시, 없으면 무시)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> KeyWarningText;

	// 키 목록 표시용 IMC 사전 등록 — 메인메뉴처럼 폰 possess 전이라 IMC 미등록인 상황 대비 (IMC_Default 지정)
	UPROPERTY(EditDefaultsOnly, Category = "MT|UI")
	TObjectPtr<UInputMappingContext> KeyRegistrationContext;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> CloseButton;

private:
	void InitTabs();

	// 탭 전환 + 탭 버튼 선택 상태 동기화
	void ShowTab(int32 Index);

	// CommonButton OnClicked()는 무인자 이벤트 → 탭별 핸들러로 인덱스 지정
	void HandleAudioTab();
	void HandleGraphicsTab();
	void HandleKeyTab();

	void InitAudio();
	void InitGraphics();
	void InitKeyBindings();

	UFUNCTION()
	void HandleMasterChanged(float Value);
	UFUNCTION()
	void HandleBGMChanged(float Value);
	UFUNCTION()
	void HandleSFXChanged(float Value);

	UFUNCTION()
	void HandleApplyGraphics();

	UFUNCTION()
	void HandleClose();

	// 볼륨 변경 공통: 저장값 갱신 + 즉시 반영
	void ApplyVolume();

	// 적용되면 true, 다른 액션과 키가 중복이면 false (적용 안 함)
	bool HandleKeyRebind(FName MappingName, FKey NewKey);
	UEnhancedInputUserSettings* GetInputUserSettings() const;

	// "이미 사용 중인 키입니다" 문구를 잠깐 표시
	void ShowKeyDuplicateWarning();

	// ResolutionCombo 인덱스 → 실제 해상도
	TArray<FIntPoint> Resolutions;

	// 경고 문구 자동 숨김 타이머
	FTimerHandle KeyWarningTimerHandle;
};
