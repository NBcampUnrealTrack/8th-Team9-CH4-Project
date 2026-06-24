#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

// MT 전용 로그 카테고리.
MEOWTRACTIVE_API DECLARE_LOG_CATEGORY_EXTERN(LogMT, Log, All);

// 콘솔 `MT.Log 1/0` 토글 상태. 디버그 출력 전에 이걸 검사한다.
MEOWTRACTIVE_API bool MTLogEnabled();
