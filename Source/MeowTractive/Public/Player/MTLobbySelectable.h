#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MTLobbySelectable.generated.h"

UINTERFACE(MinimalAPI)
class UMTLobbySelectable : public UInterface
{
	GENERATED_BODY()
};

/** 로비에서 상호작용(GA_Interact, F)으로 선택 가능한 대상. PerformSelect가 서버 권위에서 호출. */
class IMTLobbySelectable
{
	GENERATED_BODY()

public:
	// 펀치 히트 시 호출 (서버). InstigatorPawn = 때린 폰.
	virtual void OnPunchSelect(AActor* InstigatorPawn) = 0;
};

// 근접 아웃라인 헬퍼: 액터 메시에 CustomDepth 스텐실 토글 (MM_PPInteractOutline 색 1~3)
namespace MTLobbyOutline
{
	MEOWTRACTIVE_API void SetActorOutline(AActor* Target, bool bEnable, int32 StencilValue);
}
