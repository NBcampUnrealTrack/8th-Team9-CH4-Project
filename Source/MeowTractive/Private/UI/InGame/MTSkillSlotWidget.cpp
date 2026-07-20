#include "UI/InGame/MTSkillSlotWidget.h"
#include "AbilitySystemComponent.h"
#include "Player/MTGameplayAbility.h"
#include "Player/GA_Dash.h"
#include "Player/MTPlayerCharacter.h"
#include "Player/MTPlayerAttributeSet.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EnhancedInputSubsystems.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "InputCoreTypes.h"

void UMTSkillSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 키 재설정 시 SkillKeyText 갱신 구독
	if (UEnhancedInputUserSettings* Settings = GetInputUserSettings())
	{
		Settings->OnSettingsChanged.AddUniqueDynamic(this, &UMTSkillSlotWidget::HandleInputSettingsChanged);
	}
	RefreshKeyText();
}

void UMTSkillSlotWidget::NativeDestruct()
{
	if (UEnhancedInputUserSettings* Settings = GetInputUserSettings())
	{
		Settings->OnSettingsChanged.RemoveDynamic(this, &UMTSkillSlotWidget::HandleInputSettingsChanged);
	}
	Super::NativeDestruct();
}

void UMTSkillSlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (DefaultIcon)
	{
		ApplyIcon(DefaultIcon);
	}
	SetCooldownVisuals(0.f, 0.f, false);
}

void UMTSkillSlotWidget::BindAbilityByTag(UAbilitySystemComponent* InASC, FGameplayTag InAbilityTag)
{
	BoundASC = InASC;
	AbilityTag = InAbilityTag;
	SlotInputID = INDEX_NONE;
	AbilityClass = nullptr;
	bDashSlot = false;
	SpecHandle = FGameplayAbilitySpecHandle();
}

void UMTSkillSlotWidget::BindAbilityBySlot(UAbilitySystemComponent* InASC, int32 InInputID)
{
	BoundASC = InASC;
	AbilityTag = FGameplayTag();
	SlotInputID = InInputID;
	AbilityClass = nullptr;
	bDashSlot = false;
	SpecHandle = FGameplayAbilitySpecHandle();
}

void UMTSkillSlotWidget::BindAbilityByClass(UAbilitySystemComponent* InASC, TSubclassOf<UGameplayAbility> InAbilityClass)
{
	BoundASC = InASC;
	AbilityTag = FGameplayTag();
	SlotInputID = INDEX_NONE;
	AbilityClass = InAbilityClass;
	bDashSlot = false;
	SpecHandle = FGameplayAbilitySpecHandle();

	// 스펙 해석 없이 CDO 아이콘 즉시 적용 — 사망 중(ASC 없음)·패널 Collapsed(틱 정지)에도 표시됨
	if (InAbilityClass)
	{
		if (const UMTGameplayAbility* AbilityCDO = Cast<UMTGameplayAbility>(InAbilityClass->GetDefaultObject()))
		{
			if (UTexture2D* Icon = AbilityCDO->GetAbilityIcon())
			{
				ApplyIcon(Icon);
			}
		}
	}
}

void UMTSkillSlotWidget::BindDash(AMTPlayerCharacter* InCharacter)
{
	DashCharacter = InCharacter;
	BoundASC = InCharacter ? InCharacter->GetAbilitySystemComponent() : nullptr;
	AbilityClass = UGA_Dash::StaticClass();   // 아이콘은 대시 어빌리티에서 해석
	bDashSlot = true;
	SpecHandle = FGameplayAbilitySpecHandle();
}

void UMTSkillSlotWidget::SetAccentColor(const FLinearColor& InColor)
{
	if (CooldownBar)
	{
		CooldownBar->SetFillColorAndOpacity(InColor);
	}
	// CountText 색은 WBP 설정값 유지 (TeamColor 덮어쓰기 제거)
}

void UMTSkillSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 스펙 복제 지연 대비 — 아이콘 해석 전까지 재시도 (대시도 아이콘 위해 스펙 해석)
	if (!SpecHandle.IsValid())
	{
		ResolveAbilitySpec();
	}

	// 정보 패널용: 아이콘만 (항상 활성 상태 고정, 쿨다운/카운트/키 갱신 생략)
	if (bIconOnly)
	{
		SetCooldownVisuals(0.f, 0.f, false);
		return;
	}

	if (bDashSlot)
	{
		UpdateDashSlot();
		return;
	}
	UpdateAbilityCooldown();
}

void UMTSkillSlotWidget::ResolveAbilitySpec()
{
	UAbilitySystemComponent* ASC = BoundASC.Get();
	if (!ASC)
	{
		return;
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.Ability)
		{
			continue;
		}
		bool bMatch = false;
		if (SlotInputID != INDEX_NONE)
		{
			bMatch = (Spec.InputID == SlotInputID);
		}
		else if (AbilityTag.IsValid())
		{
			bMatch = Spec.Ability->GetAssetTags().HasTagExact(AbilityTag);
		}
		else if (AbilityClass)
		{
			bMatch = Spec.Ability->IsA(AbilityClass);   // 패시브·대시: 클래스 매칭
		}
		if (!bMatch)
		{
			continue;
		}

		SpecHandle = Spec.Handle;

		// 어빌리티 지정 아이콘이 있으면 교체 (없으면 DefaultIcon 유지)
		if (SkillIcon)
		{
			if (const UMTGameplayAbility* MTAbility = Cast<UMTGameplayAbility>(Spec.Ability))
			{
				if (UTexture2D* Icon = MTAbility->GetAbilityIcon())
				{
					ApplyIcon(Icon);
				}
			}
		}
		return;
	}
}

void UMTSkillSlotWidget::UpdateAbilityCooldown()
{
	UAbilitySystemComponent* ASC = BoundASC.Get();
	if (!ASC || !SpecHandle.IsValid())
	{
		SetCooldownVisuals(0.f, 0.f, false);
		return;
	}

	const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(SpecHandle);
	if (!Spec || !Spec->Ability)
	{
		SpecHandle = FGameplayAbilitySpecHandle();   // 스펙 소멸 → 다음 틱에 재검색
		SetCooldownVisuals(0.f, 0.f, false);
		return;
	}

	float Remaining = 0.f;
	float Duration = 0.f;
	Spec->Ability->GetCooldownTimeRemainingAndDuration(SpecHandle, ASC->AbilityActorInfo.Get(), Remaining, Duration);
	SetCooldownVisuals(Remaining, Duration, Remaining > 0.f);
}

void UMTSkillSlotWidget::UpdateDashSlot()
{
	const AMTPlayerCharacter* Character = DashCharacter.Get();
	UAbilitySystemComponent* ASC = BoundASC.Get();
	if (!Character || !ASC)
	{
		return;
	}

	const int32 Charges = FMath::RoundToInt(ASC->GetNumericAttribute(UMTPlayerAttributeSet::GetDashChargesAttribute()));
	if (CountText)
	{
		CountText->SetText(FText::AsNumber(Charges));
		CountText->SetVisibility(ESlateVisibility::HitTestInvisible);   // 횟수형(대시) 슬롯에서만 노출
	}

	// 재충전 진행: 완료 서버시각 − 현재 서버시각 (완충이면 EndTime=0)
	float Remaining = 0.f;
	if (Character->GetDashRechargeEndServerTime() > 0.f)
	{
		if (const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr)
		{
			Remaining = FMath::Max(0.f, Character->GetDashRechargeEndServerTime() - GS->GetServerWorldTimeSeconds());
		}
	}
	// 충전이 남아 있으면 아이콘은 사용 가능 상태로 유지
	SetCooldownVisuals(Remaining, Character->GetDashRechargeDuration(), Charges <= 0);
}

void UMTSkillSlotWidget::SetCooldownVisuals(float Remaining, float Duration, bool bDimIcon)
{
	const bool bCooling = Remaining > KINDA_SMALL_NUMBER;

	if (CooldownText)
	{
		CooldownText->SetText(bCooling ? FText::AsNumber(FMath::CeilToInt(Remaining)) : FText::GetEmpty());
		CooldownText->SetVisibility(bCooling ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (CooldownBar)
	{
		CooldownBar->SetPercent((bCooling && Duration > 0.f) ? Remaining / Duration : 0.f);
	}
	if (SkillIcon)
	{
		if (CooldownMID)
		{
			// 진행도 = 경과 비율(0→1). 머티리얼이 6시 시작·반시계로 부채꼴을 넓혀 원본을 드러냄
			const float Progress = (Duration > KINDA_SMALL_NUMBER) ? FMath::Clamp(1.f - Remaining / Duration, 0.f, 1.f) : 1.f;
			CooldownMID->SetScalarParameterValue(TEXT("Progress"), Progress);
			SkillIcon->SetColorAndOpacity(FLinearColor::White);   // 어둡게 처리는 머티리얼이 담당
		}
		else
		{
			SkillIcon->SetColorAndOpacity(bDimIcon ? FLinearColor(0.25f, 0.25f, 0.25f, 1.f) : FLinearColor::White);
		}
	}
}

void UMTSkillSlotWidget::ApplyIcon(UTexture2D* IconTexture)
{
	if (!SkillIcon)
	{
		return;
	}

	// 쿨다운 머티리얼이 있으면 MID를 만들어 브러시로 쓰고, 아이콘은 IconTex 파라미터로 전달
	if (CooldownMaterial)
	{
		if (!CooldownMID)
		{
			CooldownMID = UMaterialInstanceDynamic::Create(CooldownMaterial, this);
			SkillIcon->SetBrushFromMaterial(CooldownMID);
		}
		if (IconTexture)
		{
			CooldownMID->SetTextureParameterValue(TEXT("IconTex"), IconTexture);
		}
		return;
	}

	// 폴백: 머티리얼 미지정 시 텍스처를 브러시에 직접
	if (IconTexture)
	{
		SkillIcon->SetBrushFromTexture(IconTexture);
	}
}

UEnhancedInputUserSettings* UMTSkillSlotWidget::GetInputUserSettings() const
{
	const ULocalPlayer* LP = GetOwningLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		LP ? LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	return Subsystem ? Subsystem->GetUserSettings() : nullptr;
}

void UMTSkillSlotWidget::SetKeyMappingName(FName InMappingName)
{
	KeyMappingName = InMappingName;
	RefreshKeyText();
}

void UMTSkillSlotWidget::SetNoKeyLabel(const FText& InLabel)
{
	NoKeyLabel = InLabel;
	RefreshKeyText();
}

void UMTSkillSlotWidget::SetIconOnly(bool bInIconOnly)
{
	bIconOnly = bInIconOnly;
}

void UMTSkillSlotWidget::HandleInputSettingsChanged(UEnhancedInputUserSettings* /*Settings*/)
{
	RefreshKeyText();
}

// 키 표시명 축약: Left Shift→Shift, Left Mouse Button→LMB 등
static FText AbbreviateKeyName(const FKey& Key)
{
	if (Key == EKeys::LeftShift || Key == EKeys::RightShift)     { return FText::FromString(TEXT("Shift")); }
	if (Key == EKeys::LeftControl || Key == EKeys::RightControl) { return FText::FromString(TEXT("Ctrl")); }
	if (Key == EKeys::LeftAlt || Key == EKeys::RightAlt)         { return FText::FromString(TEXT("Alt")); }
	if (Key == EKeys::LeftMouseButton)   { return FText::FromString(TEXT("LMB")); }
	if (Key == EKeys::RightMouseButton)  { return FText::FromString(TEXT("RMB")); }
	if (Key == EKeys::MiddleMouseButton) { return FText::FromString(TEXT("MMB")); }
	if (Key == EKeys::SpaceBar)          { return FText::FromString(TEXT("Space")); }
	return Key.GetDisplayName(/*bLongDisplayName=*/false);
}

void UMTSkillSlotWidget::RefreshKeyText()
{
	if (!SkillKeyText)
	{
		return;
	}
	if (KeyMappingName.IsNone())
	{
		// 키 없는 슬롯(패시브 등) — 고정 라벨이 있으면 표시, 없으면 숨김
		if (!NoKeyLabel.IsEmpty())
		{
			SkillKeyText->SetText(NoKeyLabel);
			SkillKeyText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			SkillKeyText->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	// 매핑 이름으로 First 슬롯의 현재 키를 조회
	FKey CurrentKey;
	if (UEnhancedInputUserSettings* Settings = GetInputUserSettings())
	{
		if (const UEnhancedPlayerMappableKeyProfile* Profile = Settings->GetActiveKeyProfile())
		{
			if (const FKeyMappingRow* Row = Profile->GetPlayerMappingRows().Find(KeyMappingName))
			{
				for (const FPlayerKeyMapping& Mapping : Row->Mappings)
				{
					if (Mapping.GetSlot() == EPlayerMappableKeySlot::First)
					{
						CurrentKey = Mapping.GetCurrentKey();
						break;
					}
				}
			}
		}
	}

	if (CurrentKey.IsValid())
	{
		SkillKeyText->SetText(AbbreviateKeyName(CurrentKey));
		SkillKeyText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		SkillKeyText->SetVisibility(ESlateVisibility::Collapsed);
	}
}
