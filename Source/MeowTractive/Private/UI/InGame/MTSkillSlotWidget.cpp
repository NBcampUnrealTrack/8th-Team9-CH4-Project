#include "UI/InGame/MTSkillSlotWidget.h"
#include "AbilitySystemComponent.h"
#include "Player/MTGameplayAbility.h"
#include "Player/MTPlayerCharacter.h"
#include "Player/MTPlayerAttributeSet.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "GameFramework/GameStateBase.h"

void UMTSkillSlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (SkillIcon && DefaultIcon)
	{
		SkillIcon->SetBrushFromTexture(DefaultIcon);
	}
	SetCooldownVisuals(0.f, 0.f, false);
}

void UMTSkillSlotWidget::BindAbilityByTag(UAbilitySystemComponent* InASC, FGameplayTag InAbilityTag)
{
	BoundASC = InASC;
	AbilityTag = InAbilityTag;
	SlotInputID = INDEX_NONE;
	bDashSlot = false;
	SpecHandle = FGameplayAbilitySpecHandle();
}

void UMTSkillSlotWidget::BindAbilityBySlot(UAbilitySystemComponent* InASC, int32 InInputID)
{
	BoundASC = InASC;
	AbilityTag = FGameplayTag();
	SlotInputID = InInputID;
	bDashSlot = false;
	SpecHandle = FGameplayAbilitySpecHandle();
}

void UMTSkillSlotWidget::BindDash(AMTPlayerCharacter* InCharacter)
{
	DashCharacter = InCharacter;
	BoundASC = InCharacter ? InCharacter->GetAbilitySystemComponent() : nullptr;
	bDashSlot = true;
	SpecHandle = FGameplayAbilitySpecHandle();
}

void UMTSkillSlotWidget::SetAccentColor(const FLinearColor& InColor)
{
	if (CooldownBar)
	{
		CooldownBar->SetFillColorAndOpacity(InColor);
	}
	if (CountText)
	{
		CountText->SetColorAndOpacity(FSlateColor(InColor));
	}
}

void UMTSkillSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bDashSlot)
	{
		UpdateDashSlot();
		return;
	}

	if (!SpecHandle.IsValid())
	{
		ResolveAbilitySpec();
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
		const bool bMatch = (SlotInputID != INDEX_NONE)
			? (Spec.InputID == SlotInputID)
			: (AbilityTag.IsValid() && Spec.Ability->GetAssetTags().HasTagExact(AbilityTag));
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
					SkillIcon->SetBrushFromTexture(Icon);
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
		SkillIcon->SetColorAndOpacity(bDimIcon ? FLinearColor(0.25f, 0.25f, 0.25f, 1.f) : FLinearColor::White);
	}
}
