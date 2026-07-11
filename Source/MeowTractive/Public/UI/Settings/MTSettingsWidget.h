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
class UEnhancedInputUserSettings;

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
	UFUNCTION()
	void HandleKeySelected(FInputChord Chord);

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

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

private:
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
