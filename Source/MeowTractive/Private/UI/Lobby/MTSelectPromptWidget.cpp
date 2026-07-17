#include "UI/Lobby/MTSelectPromptWidget.h"

#include "Components/TextBlock.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "UserSettings/EnhancedInputUserSettings.h"

void UMTSelectPromptWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RefreshPrompt();
	// 설정 창에서 키를 바꿔도 곧 반영되게 주기 갱신 (로비 위젯 폴링 관행)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RefreshTimer, this, &UMTSelectPromptWidget::RefreshPrompt, 1.f, true);
	}
}

void UMTSelectPromptWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}
	Super::NativeDestruct();
}

void UMTSelectPromptWidget::SetActionLabel(const FText& InLabel)
{
	ActionLabel = InLabel;
	RefreshPrompt();
}

void UMTSelectPromptWidget::RefreshPrompt()
{
	if (PromptText)
	{
		PromptText->SetText(FText::Format(
			NSLOCTEXT("MT", "SelectPromptFormat", "[{0}] {1}"), ResolveKeyText(), ActionLabel));
	}
}

FText UMTSelectPromptWidget::ResolveKeyText() const
{
	// WidgetComponent 소속이라 OwningLocalPlayer가 없을 수 있음 → 첫 로컬 플레이어 폴백
	const ULocalPlayer* LP = GetOwningLocalPlayer();
	if (!LP)
	{
		if (const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			LP = PC->GetLocalPlayer();
		}
	}

	const UEnhancedInputLocalPlayerSubsystem* Subsystem =
		LP ? LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	const UEnhancedInputUserSettings* Settings = Subsystem ? Subsystem->GetUserSettings() : nullptr;
	const UEnhancedPlayerMappableKeyProfile* Profile = Settings ? Settings->GetActiveKeyProfile() : nullptr;
	if (Profile)
	{
		if (const FKeyMappingRow* Row = Profile->GetPlayerMappingRows().Find(KeyMappingName))
		{
			for (const FPlayerKeyMapping& Mapping : Row->Mappings)
			{
				if (Mapping.GetSlot() == EPlayerMappableKeySlot::First)
				{
					return Mapping.GetCurrentKey().GetDisplayName(false);
				}
			}
		}
	}
	return FallbackKeyText;
}
