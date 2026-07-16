#include "UI/Settings/MTSettingsWidget.h"
#include "Game/MTGameInstance.h"
#include "Game/MTGameUserSettings.h"
#include "Components/Slider.h"
#include "Components/ProgressBar.h"
#include "Components/ComboBoxString.h"
#include "Components/CheckBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "CommonAnimatedSwitcher.h"
#include "CommonButtonBase.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UMTKeySelector::Init(FName InMappingName)
{
	MappingName = InMappingName;
	OnKeySelected.AddUniqueDynamic(this, &UMTKeySelector::HandleChordSelected);
	SetAllowGamepadKeys(false);
	SetAllowModifierKeys(false);
	SetEscapeKeys({ EKeys::Escape });   // ESC = 선택 취소
}

void UMTKeySelector::HandleChordSelected(FInputChord Chord)
{
	if (Chord.Key.IsValid() && Chord.Key != EKeys::Escape)
	{
		OnKeyRebind.ExecuteIfBound(MappingName, Chord.Key);
	}
}

void UMTKeyRowWidget::Setup(FName InMappingName, const FText& InLabelText, const FKey& InCurrentKey)
{
	CurrentKey = InCurrentKey;
	if (Label)
	{
		Label->SetText(InLabelText);
	}
	if (Selector)
	{
		Selector->Init(InMappingName);
		Selector->SetSelectedKey(FInputChord(InCurrentKey));
		Selector->OnKeyRebind.BindUObject(this, &UMTKeyRowWidget::HandleSelectorRebind);
	}
}

void UMTKeyRowWidget::HandleSelectorRebind(FName MappingName, FKey NewKey)
{
	const bool bApplied = OnKeyRebind.IsBound() && OnKeyRebind.Execute(MappingName, NewKey);
	if (bApplied)
	{
		CurrentKey = NewKey;
	}
	else if (Selector)
	{
		// 중복 등으로 거부됨 → 셀렉터 표시를 원래 키로 되돌림
		Selector->SetSelectedKey(FInputChord(CurrentKey));
	}
}

void UMTSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);   // ESC 수신용

	if (CloseButton)
	{
		CloseButton->OnClicked().AddUObject(this, &UMTSettingsWidget::HandleClose);
	}
	if (ApplyGraphicsButton)
	{
		ApplyGraphicsButton->OnClicked().AddUObject(this, &UMTSettingsWidget::HandleApplyGraphics);
	}

	if (KeyWarningText)
	{
		// 문구를 미리 채워 Hidden이 예약할 공간을 확정 (레이아웃 흔들림 방지)
		KeyWarningText->SetText(FText::FromString(TEXT("이미 사용 중인 키입니다")));
		KeyWarningText->SetVisibility(ESlateVisibility::Hidden);
	}

	InitAudio();
	InitGraphics();
	InitKeyBindings();
	InitTabs();
}

void UMTSettingsWidget::NativeDestruct()
{
	OnClosed.Broadcast();   // 어떤 경로로 닫혀도 여는 쪽이 복원할 수 있게

	// CommonButtonBase는 non-dynamic 이벤트 → 수동 해제
	if (CloseButton)
	{
		CloseButton->OnClicked().RemoveAll(this);
	}
	if (AudioTabButton)
	{
		AudioTabButton->OnClicked().RemoveAll(this);
	}
	if (GraphicsTabButton)
	{
		GraphicsTabButton->OnClicked().RemoveAll(this);
	}
	if (KeyTabButton)
	{
		KeyTabButton->OnClicked().RemoveAll(this);
	}
	if (ApplyGraphicsButton)
	{
		ApplyGraphicsButton->OnClicked().RemoveAll(this);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(KeyWarningTimerHandle);
	}
	Super::NativeDestruct();
}

void UMTSettingsWidget::InitTabs()
{
	if (!TabSwitcher)
	{
		return;   // 탭 없는 레이아웃 (기존 3열) 그대로 동작
	}

	if (AudioTabButton)
	{
		AudioTabButton->OnClicked().AddUObject(this, &UMTSettingsWidget::HandleAudioTab);
	}
	if (GraphicsTabButton)
	{
		GraphicsTabButton->OnClicked().AddUObject(this, &UMTSettingsWidget::HandleGraphicsTab);
	}
	if (KeyTabButton)
	{
		KeyTabButton->OnClicked().AddUObject(this, &UMTSettingsWidget::HandleKeyTab);
	}

	ShowTab(0);   // 기본: 볼륨 탭
}

void UMTSettingsWidget::ShowTab(int32 Index)
{
	if (!TabSwitcher)
	{
		return;
	}
	TabSwitcher->SetActiveWidgetIndex(Index);

	// 탭 버튼 선택 상태 동기화 (WBP에서 bToggleable=true면 Selected 스타일 표시)
	UCommonButtonBase* Tabs[] = { AudioTabButton, GraphicsTabButton, KeyTabButton };
	for (int32 i = 0; i < UE_ARRAY_COUNT(Tabs); ++i)
	{
		if (Tabs[i])
		{
			Tabs[i]->SetIsSelected(i == Index, /*bGiveClickFeedback=*/false);
		}
	}

	// 적용 버튼은 그래픽 탭에서만 — Hidden이라 다른 탭에서도 자리는 유지 (레이아웃 흔들림 방지)
	if (ApplyGraphicsButton)
	{
		ApplyGraphicsButton->SetVisibility(Index == 1 ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
}

void UMTSettingsWidget::HandleAudioTab()
{
	ShowTab(0);
}

void UMTSettingsWidget::HandleGraphicsTab()
{
	ShowTab(1);
}

void UMTSettingsWidget::HandleKeyTab()
{
	ShowTab(2);
}

FReply UMTSettingsWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		HandleClose();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UMTSettingsWidget::InitAudio()
{
	const UMTGameUserSettings* Settings = UMTGameUserSettings::GetMTSettings();
	if (!Settings)
	{
		return;
	}
	if (MasterSlider)
	{
		MasterSlider->SetValue(Settings->GetMasterVolume());
		MasterSlider->OnValueChanged.AddDynamic(this, &UMTSettingsWidget::HandleMasterChanged);
	}
	if (BGMSlider)
	{
		BGMSlider->SetValue(Settings->GetBGMVolume());
		BGMSlider->OnValueChanged.AddDynamic(this, &UMTSettingsWidget::HandleBGMChanged);
	}
	if (SFXSlider)
	{
		SFXSlider->SetValue(Settings->GetSFXVolume());
		SFXSlider->OnValueChanged.AddDynamic(this, &UMTSettingsWidget::HandleSFXChanged);
	}

	// 저장된 볼륨으로 채움 바 초기 상태 맞춤 (이후엔 슬라이더 콜백이 갱신)
	if (PB_MasterSlider)
	{
		PB_MasterSlider->SetPercent(Settings->GetMasterVolume());
	}
	if (PB_BGMSlider)
	{
		PB_BGMSlider->SetPercent(Settings->GetBGMVolume());
	}
	if (PB_SFXSlider)
	{
		PB_SFXSlider->SetPercent(Settings->GetSFXVolume());
	}
}

void UMTSettingsWidget::HandleMasterChanged(float Value)
{
	if (UMTGameUserSettings* Settings = UMTGameUserSettings::GetMTSettings())
	{
		Settings->SetMasterVolume(Value);
	}
	if (PB_MasterSlider)
	{
		PB_MasterSlider->SetPercent(Value);
	}
	ApplyVolume();
}

void UMTSettingsWidget::HandleBGMChanged(float Value)
{
	if (UMTGameUserSettings* Settings = UMTGameUserSettings::GetMTSettings())
	{
		Settings->SetBGMVolume(Value);
	}
	if (PB_BGMSlider)
	{
		PB_BGMSlider->SetPercent(Value);
	}
	ApplyVolume();
}

void UMTSettingsWidget::HandleSFXChanged(float Value)
{
	if (UMTGameUserSettings* Settings = UMTGameUserSettings::GetMTSettings())
	{
		Settings->SetSFXVolume(Value);
	}
	if (PB_SFXSlider)
	{
		PB_SFXSlider->SetPercent(Value);
	}
	ApplyVolume();
}

void UMTSettingsWidget::ApplyVolume()
{
	if (UMTGameInstance* GI = Cast<UMTGameInstance>(GetGameInstance()))
	{
		GI->ApplyAudioSettings();
	}
}

void UMTSettingsWidget::InitGraphics()
{
	UGameUserSettings* Settings = UMTGameUserSettings::GetMTSettings();
	if (!Settings)
	{
		return;
	}

	if (WindowModeCombo)
	{
		WindowModeCombo->ClearOptions();
		WindowModeCombo->AddOption(TEXT("전체화면"));
		WindowModeCombo->AddOption(TEXT("테두리 없는 창"));
		WindowModeCombo->AddOption(TEXT("창모드"));
		WindowModeCombo->SetSelectedIndex((int32)Settings->GetFullscreenMode());
	}

	if (ResolutionCombo)
	{
		ResolutionCombo->ClearOptions();
		Resolutions.Reset();
		UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
		const FIntPoint Current = Settings->GetScreenResolution();
		int32 CurrentIndex = INDEX_NONE;
		for (int32 i = 0; i < Resolutions.Num(); ++i)
		{
			ResolutionCombo->AddOption(FString::Printf(TEXT("%d x %d"), Resolutions[i].X, Resolutions[i].Y));
			if (Resolutions[i] == Current)
			{
				CurrentIndex = i;
			}
		}
		if (CurrentIndex != INDEX_NONE)
		{
			ResolutionCombo->SetSelectedIndex(CurrentIndex);
		}
	}

	if (QualityCombo)
	{
		QualityCombo->ClearOptions();
		QualityCombo->AddOption(TEXT("낮음"));
		QualityCombo->AddOption(TEXT("중간"));
		QualityCombo->AddOption(TEXT("높음"));
		QualityCombo->AddOption(TEXT("최고"));
		const int32 Quality = Settings->GetOverallScalabilityLevel();   // 혼합 설정이면 -1
		QualityCombo->SetSelectedIndex(Quality >= 0 ? FMath::Min(Quality, 3) : 2);
	}

	if (VSyncCheck)
	{
		VSyncCheck->SetIsChecked(Settings->IsVSyncEnabled());
	}
}

void UMTSettingsWidget::HandleApplyGraphics()
{
	UGameUserSettings* Settings = UMTGameUserSettings::GetMTSettings();
	if (!Settings)
	{
		return;
	}

	if (WindowModeCombo)
	{
		Settings->SetFullscreenMode((EWindowMode::Type)FMath::Clamp(WindowModeCombo->GetSelectedIndex(), 0, 2));
	}
	if (ResolutionCombo && Resolutions.IsValidIndex(ResolutionCombo->GetSelectedIndex()))
	{
		Settings->SetScreenResolution(Resolutions[ResolutionCombo->GetSelectedIndex()]);
	}
	if (QualityCombo && QualityCombo->GetSelectedIndex() >= 0)
	{
		Settings->SetOverallScalabilityLevel(QualityCombo->GetSelectedIndex());
	}
	if (VSyncCheck)
	{
		Settings->SetVSyncEnabled(VSyncCheck->IsChecked());
	}

	Settings->ApplySettings(false);
	Settings->SaveSettings();
}

UEnhancedInputUserSettings* UMTSettingsWidget::GetInputUserSettings() const
{
	const ULocalPlayer* LP = GetOwningLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		LP ? LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	return Subsystem ? Subsystem->GetUserSettings() : nullptr;
}

void UMTSettingsWidget::InitKeyBindings()
{
	UEnhancedInputUserSettings* Settings = GetInputUserSettings();
	if (Settings && KeyRegistrationContext)
	{
		// 미등록이면 키 행이 비어 보임 — 멱등이라 중복 호출 무해
		Settings->RegisterInputMappingContext(KeyRegistrationContext);
	}
	const UEnhancedPlayerMappableKeyProfile* Profile = Settings ? Settings->GetActiveKeyProfile() : nullptr;
	if (!KeyListBox || !Profile)
	{
		return;   // bEnableUserSettings 꺼짐 등 — 키 섹션 비움
	}

	if (!KeyRowWidgetClass)
	{
		return;   // WBP_MTSettings에 행 위젯 클래스 미지정
	}

	KeyListBox->ClearChildren();

	const TMap<FName, FKeyMappingRow>& Rows = Profile->GetPlayerMappingRows();

	// 매핑 이름으로 First 슬롯 행 하나 생성 (이미 만든 이름은 건너뜀)
	TSet<FName> Added;
	auto AddRow = [&](FName MappingName)
	{
		if (Added.Contains(MappingName))
		{
			return;
		}
		const FKeyMappingRow* Row = Rows.Find(MappingName);
		if (!Row)
		{
			return;
		}
		for (const FPlayerKeyMapping& Mapping : Row->Mappings)
		{
			if (Mapping.GetSlot() != EPlayerMappableKeySlot::First)
			{
				continue;   // 1키 = 1행
			}

			UMTKeyRowWidget* RowWidget = CreateWidget<UMTKeyRowWidget>(this, KeyRowWidgetClass);
			RowWidget->Setup(MappingName, Mapping.GetDisplayName(), Mapping.GetCurrentKey());
			RowWidget->OnKeyRebind.BindUObject(this, &UMTSettingsWidget::HandleKeyRebind);

			if (UVerticalBoxSlot* RowSlot = KeyListBox->AddChildToVerticalBox(RowWidget))
			{
				RowSlot->SetPadding(FMargin(0.f, 4.f));
			}
			Added.Add(MappingName);
			break;
		}
	};

	// IMC에 정의된 순서대로 생성 — TMap 순회는 순서 불안정이라 행이 뒤섞이는 것 방지
	if (KeyRegistrationContext)
	{
		for (const FEnhancedActionKeyMapping& Mapping : KeyRegistrationContext->GetMappings())
		{
			if (Mapping.IsPlayerMappable())
			{
				AddRow(Mapping.GetMappingName());
			}
		}
	}

	// IMC에 없던(혹은 IMC 미지정) 나머지는 뒤에 붙임
	for (const TPair<FName, FKeyMappingRow>& Row : Rows)
	{
		AddRow(Row.Key);
	}
}

bool UMTSettingsWidget::HandleKeyRebind(FName MappingName, FKey NewKey)
{
	UEnhancedInputUserSettings* Settings = GetInputUserSettings();
	if (!Settings)
	{
		return false;
	}

	// 다른 액션이 이미 같은 키를 쓰고 있으면 거부 (중복 방지)
	if (const UEnhancedPlayerMappableKeyProfile* Profile = Settings->GetActiveKeyProfile())
	{
		for (const TPair<FName, FKeyMappingRow>& Row : Profile->GetPlayerMappingRows())
		{
			if (Row.Key == MappingName)
			{
				continue;   // 같은 액션은 검사 제외 (자기 키 재선택 허용)
			}
			for (const FPlayerKeyMapping& Mapping : Row.Value.Mappings)
			{
				if (Mapping.GetSlot() == EPlayerMappableKeySlot::First && Mapping.GetCurrentKey() == NewKey)
				{
					ShowKeyDuplicateWarning();
					return false;   // 중복 → 적용 안 함 (호출측이 원래 키로 되돌림)
				}
			}
		}
	}

	FMapPlayerKeyArgs Args;
	Args.MappingName = MappingName;
	Args.Slot = EPlayerMappableKeySlot::First;
	Args.NewKey = NewKey;

	FGameplayTagContainer FailureReason;
	Settings->MapPlayerKey(Args, FailureReason);
	Settings->ApplySettings();
	Settings->AsyncSaveSettings();
	return true;
}

void UMTSettingsWidget::ShowKeyDuplicateWarning()
{
	if (!KeyWarningText)
	{
		return;   // WBP에 경고 문구 위젯이 없으면 무시
	}

	// Hidden ↔ HitTestInvisible 만 토글 (Collapsed 아님) → 공간 유지, 크기 안 흔들림
	KeyWarningText->SetVisibility(ESlateVisibility::HitTestInvisible);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(KeyWarningTimerHandle);
		World->GetTimerManager().SetTimer(
			KeyWarningTimerHandle,
			[WeakThis = TWeakObjectPtr<UMTSettingsWidget>(this)]()
			{
				if (WeakThis.IsValid() && WeakThis->KeyWarningText)
				{
					WeakThis->KeyWarningText->SetVisibility(ESlateVisibility::Hidden);
				}
			},
			2.0f, /*bLoop=*/false);
	}
}

void UMTSettingsWidget::HandleClose()
{
	// 볼륨 저장 (그래픽/키는 각자 저장됨)
	if (UMTGameUserSettings* Settings = UMTGameUserSettings::GetMTSettings())
	{
		Settings->SaveSettings();
	}
	RemoveFromParent();
}
