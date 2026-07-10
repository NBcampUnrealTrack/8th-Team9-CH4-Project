#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MTLobbySelectable.generated.h"

UINTERFACE(MinimalAPI)
class UMTLobbySelectable : public UInterface
{
	GENERATED_BODY()
};

/** 로비에서 근접공격(MeowPunch)으로 상호작용 가능한 대상. PerformHit이 서버 권위에서 호출. */
class IMTLobbySelectable
{
	GENERATED_BODY()

public:
	// 펀치 히트 시 호출 (서버). InstigatorPawn = 때린 폰.
	virtual void OnPunchSelect(AActor* InstigatorPawn) = 0;
};
