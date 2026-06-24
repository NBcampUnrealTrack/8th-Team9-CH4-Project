#include "Game/MTLog.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY(LogMT);

// 콘솔: `MT.Log 1` 켜기 / `MT.Log 0` 끄기
static TAutoConsoleVariable<int32> CVarMTLog(
	TEXT("MT.Log"),
	0,
	TEXT("MT 디버그 로그/화면 출력 토글 (1=켜기, 0=끄기)"),
	ECVF_Default);

bool MTLogEnabled()
{
	return CVarMTLog.GetValueOnGameThread() != 0;
}
