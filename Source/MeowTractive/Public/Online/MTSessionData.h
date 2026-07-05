#pragma once

#include "CoreMinimal.h"
#include "OnlineSessionSettings.h"
#include "MTSessionData.generated.h"

/** ListView용 세션 1개 데이터. BP엔 표시용 필드만, OSS 결과는 C++ 전용. */
UCLASS(BlueprintType)
class MEOWTRACTIVE_API UMTSessionData : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "MT|Session")
	FString ServerName;

	UPROPERTY(BlueprintReadOnly, Category = "MT|Session")
	int32 PingMs = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MT|Session")
	int32 OpenSlots = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MT|Session")
	int32 MaxSlots = 0;

	// 현재 참여 인원 (Max - Open)
	UPROPERTY(BlueprintReadOnly, Category = "MT|Session")
	int32 CurrentPlayers = 0;

	// C++ 전용 — JoinSession에 사용 (BP 비호환)
	FOnlineSessionSearchResult Result;
};
