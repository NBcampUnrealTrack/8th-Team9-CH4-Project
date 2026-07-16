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

// 방 게임모드 (세션 설정으로 광고. 팀전/고양이전투는 자리만)
UENUM(BlueprintType)
enum class EMTRoomGameMode : uint8
{
	FFA      UMETA(DisplayName = "개인전"),
	Team     UMETA(DisplayName = "팀전"),
	CatFight UMETA(DisplayName = "고양이 전투"),
};

// 매치 맵 선택 (콤보 인덱스 = enum 값 — UI 옵션 순서와 일치 유지)
UENUM(BlueprintType)
enum class EMTRoomMap : uint8
{
	Park     UMETA(DisplayName = "공원"),
	Random   UMETA(DisplayName = "랜덤"),
};

// 방 설정 (생성/로비 변경 공용)
USTRUCT(BlueprintType)
struct FMTRoomSettings
{
	GENERATED_BODY()

	// 비어 있으면 호스트 닉네임 사용
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "MT|Session")
	FString RoomName;

	// 비어 있으면 공개방
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "MT|Session")
	FString Password;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "MT|Session")
	EMTRoomGameMode GameMode = EMTRoomGameMode::FFA;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "MT|Session")
	EMTRoomMap Map = EMTRoomMap::Park;

	bool HasPassword() const { return !Password.IsEmpty(); }
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
