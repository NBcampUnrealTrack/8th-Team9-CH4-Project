#include "Game/MTGameUserSettings.h"
#include "Engine/Engine.h"

UMTGameUserSettings* UMTGameUserSettings::GetMTSettings()
{
	return GEngine ? Cast<UMTGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}
