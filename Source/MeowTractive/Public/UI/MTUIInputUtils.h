#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

class ULocalPlayer;

namespace MTUIInput
{
	// EnhancedInput 유저 설정에서 매핑 이름의 First 슬롯 현재 키 조회 (없으면 Fallback)
	MEOWTRACTIVE_API FKey GetPlayerMappedKey(const ULocalPlayer* LocalPlayer, FName MappingName, const FKey& Fallback);
}
