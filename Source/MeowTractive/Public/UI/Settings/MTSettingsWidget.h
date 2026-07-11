#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/InputKeySelector.h"
#include "MTSettingsWidget.generated.h"

class USlider;
class UComboBoxString;
class UCheckBox;
class UButton;
class UVerticalBox;
class UCommonAnimatedSwitcher;
class UCommonButtonBase;
class UEnhancedInputUserSettings;
class UInputMappingContext;

// 키 셀렉터 → 설정 위젯으로 결과 전달 (어느 매핑인지 포함)
DECLARE_DELEGATE_TwoParams(FMTOnKeyRebind, FName /*MappingName*/, FKey /*NewKey*/);

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

/** 설정 화면: 볼륨(마스터/배경음/효과음) + 그래픽(창모드/해상도/품질/수직동기) + 키 재바인딩.
 *  일시정지 메뉴·메인메뉴 공용. 볼륨은 즉시 적용, 그래픽은 적용 버튼, 키는 선택 즉시 저장. */
UCLASS()
class MEOWTRACTIVE_API UMTSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

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
	TObjectPtr<UButton> ApplyGraphicsButton;

	// --- 키 재바인딩 (행을 C++이 동적 생성) ---
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> KeyListBox;

	// 키 행 라벨/셀렉터 폰트 (WBP 디폴트에서 지정. 미지정 시 엔진 기본)
	UPROPERTY(EditAnywhere, Category = "MT|UI")
	FSlateFontInfo KeyRowFont;

	// 키 행 라벨 색 (흰 패널 위 다크로즈)
	UPROPERTY(EditAnywhere, Category = "MT|UI")
	FLinearColor KeyRowLabelColor = FLinearColor(0.38f, 0.20f, 0.26f, 1.f);

	// 키 셀렉터 버튼 색 (핑크)
	UPROPERTY(EditAnywhere, Category = "MT|UI")
	FLinearColor KeySelectorColor = FLinearColor(0.95f, 0.45f, 0.60f, 1.f);

	// 키 목록 표시용 IMC 사전 등록 — 메인메뉴처럼 폰 possess 전이라 IMC 미등록인 상황 대비 (IMC_Default 지정)
	UPROPERTY(EditDefaultsOnly, Category = "MT|UI")
	TObjectPtr<UInputMappingContext> KeyRegistrationContext;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

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

	void HandleKeyRebind(FName MappingName, FKey NewKey);
	UEnhancedInputUserSettings* GetInputUserSettings() const;

	// ResolutionCombo 인덱스 → 실제 해상도
	TArray<FIntPoint> Resolutions;
};
