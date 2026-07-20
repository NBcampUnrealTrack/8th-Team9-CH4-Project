#include "UI/MTUIInputUtils.h"
#include "EnhancedInputSubsystems.h"
#include "UserSettings/EnhancedInputUserSettings.h"

FKey MTUIInput::GetPlayerMappedKey(const ULocalPlayer* LocalPlayer, FName MappingName, const FKey& Fallback)
{
	const UEnhancedInputLocalPlayerSubsystem* Subsystem =
		LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	const UEnhancedInputUserSettings* Settings = Subsystem ? Subsystem->GetUserSettings() : nullptr;
	const UEnhancedPlayerMappableKeyProfile* Profile = Settings ? Settings->GetActiveKeyProfile() : nullptr;
	if (!Profile)
	{
		return Fallback;
	}

	if (const FKeyMappingRow* Row = Profile->GetPlayerMappingRows().Find(MappingName))
	{
		for (const FPlayerKeyMapping& Mapping : Row->Mappings)
		{
			if (Mapping.GetSlot() == EPlayerMappableKeySlot::First && Mapping.GetCurrentKey().IsValid())
			{
				return Mapping.GetCurrentKey();
			}
		}
	}
	return Fallback;
}
