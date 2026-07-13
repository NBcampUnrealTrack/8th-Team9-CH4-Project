#include "UI/Settings/MTSettingsWidget.h"
#include "Game/MTGameInstance.h"
#include "Game/MTGameUserSettings.h"
#include "Components/Slider.h"
#include "Components/ComboBoxString.h"
#include "Components/CheckBox.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "CommonAnimatedSwitcher.h"
#include "CommonButtonBase.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"

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

void UMTSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);   // ESC 수신용

	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &UMTSettingsWidget::HandleClose);
	}
	if (ApplyGraphicsButton)
	{
		ApplyGraphicsButton->OnClicked.AddDynamic(this, &UMTSettingsWidget::HandleApplyGraphics);
	}

	InitAudio();
	InitGraphics();
	InitKeyBindings();
	InitTabs();
}

void UMTSettingsWidget::NativeDestruct()
{
	// CommonButtonBase는 non-dynamic 이벤트 → 수동 해제
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
}

void UMTSettingsWidget::HandleMasterChanged(float Value)
{
	if (UMTGameUserSettings* Settings = UMTGameUserSettings::GetMTSettings())
	{
		Settings->SetMasterVolume(Value);
	}
	ApplyVolume();
}

void UMTSettingsWidget::HandleBGMChanged(float Value)
{
	if (UMTGameUserSettings* Settings = UMTGameUserSettings::GetMTSettings())
	{
		Settings->SetBGMVolume(Value);
	}
	ApplyVolume();
}

void UMTSettingsWidget::HandleSFXChanged(float Value)
{
	if (UMTGameUserSettings* Settings = UMTGameUserSettings::GetMTSettings())
	{
		Settings->SetSFXVolume(Value);
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

	KeyListBox->ClearChildren();

	for (const TPair<FName, FKeyMappingRow>& Row : Profile->GetPlayerMappingRows())
	{
		for (const FPlayerKeyMapping& Mapping : Row.Value.Mappings)
		{
			if (Mapping.GetSlot() != EPlayerMappableKeySlot::First)
			{
				continue;   // 1키 = 1행
			}

			// 행: [스킬 이름 | 키 셀렉터]
			UHorizontalBox* RowBox = WidgetTree->ConstructWidget<UHorizontalBox>();
			UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
			Label->SetText(Mapping.GetDisplayName());
			if (KeyRowFont.HasValidFont())
			{
				Label->SetFont(KeyRowFont);
			}
			Label->SetColorAndOpacity(FSlateColor(KeyRowLabelColor));

			UMTKeySelector* Selector = WidgetTree->ConstructWidget<UMTKeySelector>();
			Selector->Init(Row.Key);
			Selector->SetSelectedKey(FInputChord(Mapping.GetCurrentKey()));
			Selector->OnKeyRebind.BindUObject(this, &UMTSettingsWidget::HandleKeyRebind);

			// 톤앤매너: 핑크 라운드 버튼 + 폰트
			{
				auto MakeRounded = [](const FLinearColor& Color)
				{
					FSlateBrush Brush;
					Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
					Brush.TintColor = FSlateColor(Color);
					Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(12.f, 12.f, 12.f, 12.f));
					Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
					return Brush;
				};
				FButtonStyle BtnStyle;
				BtnStyle.SetNormal(MakeRounded(KeySelectorColor));
				BtnStyle.SetHovered(MakeRounded(KeySelectorColor * 1.15f));
				BtnStyle.SetPressed(MakeRounded(KeySelectorColor * 0.8f));
				Selector->SetButtonStyle(BtnStyle);

				FTextBlockStyle KeyTextStyle;
				if (KeyRowFont.HasValidFont())
				{
					KeyTextStyle.SetFont(KeyRowFont);
				}
				KeyTextStyle.SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.96f, 0.92f, 1.f)));
				Selector->SetTextStyle(KeyTextStyle);
				Selector->SetMargin(FMargin(14.f, 6.f));
				Selector->SetKeySelectionText(FText::FromString(TEXT("아무 키나 누르세요...")));
				Selector->SetNoKeySpecifiedText(FText::FromString(TEXT("없음")));
			}

			if (UHorizontalBoxSlot* LabelSlot = RowBox->AddChildToHorizontalBox(Label))
			{
				LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				LabelSlot->SetVerticalAlignment(VAlign_Center);
			}
			if (UHorizontalBoxSlot* SelectorSlot = RowBox->AddChildToHorizontalBox(Selector))
			{
				SelectorSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				SelectorSlot->SetVerticalAlignment(VAlign_Center);
			}
			if (UVerticalBoxSlot* RowSlot = KeyListBox->AddChildToVerticalBox(RowBox))
			{
				RowSlot->SetPadding(FMargin(0.f, 4.f));
			}
			break;
		}
	}
}

void UMTSettingsWidget::HandleKeyRebind(FName MappingName, FKey NewKey)
{
	UEnhancedInputUserSettings* Settings = GetInputUserSettings();
	if (!Settings)
	{
		return;
	}

	FMapPlayerKeyArgs Args;
	Args.MappingName = MappingName;
	Args.Slot = EPlayerMappableKeySlot::First;
	Args.NewKey = NewKey;

	FGameplayTagContainer FailureReason;
	Settings->MapPlayerKey(Args, FailureReason);
	Settings->ApplySettings();
	Settings->AsyncSaveSettings();
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
