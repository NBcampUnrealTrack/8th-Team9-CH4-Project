#pragma once

#include "CoreMinimal.h"
#include "MTTypes.generated.h"

// 고양이 종류 (선택지·밸런스는 별도 담당 — 지금은 자리만)
UENUM(BlueprintType)
enum class EMTCatType : uint8
{
	None     UMETA(DisplayName = "None"),
	Fatty    UMETA(DisplayName = "뚱냥이"),
	Spotted  UMETA(DisplayName = "점박이"),
	Mackerel UMETA(DisplayName = "고등어"),
};

// 매치 단계 (M3에서 GameState가 사용)
UENUM(BlueprintType)
enum class EMTMatchPhase : uint8
{
	Lobby,
	WaitingLoad,
	InProgress,
	PostMatch,
};

USTRUCT(BlueprintType)
struct FMTPlayerResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString PlayerName;

    UPROPERTY(BlueprintReadOnly)
    FLinearColor TeamColor = FLinearColor::White;

    UPROPERTY(BlueprintReadOnly)
    int32 AttractedCount = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 Rank = 0;
};
