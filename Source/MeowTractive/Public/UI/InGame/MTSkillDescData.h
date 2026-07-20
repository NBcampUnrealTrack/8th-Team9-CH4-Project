#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Game/MTTypes.h"
#include "MTSkillDescData.generated.h"

// 한 고양이의 패시브/스킬1/스킬2 설명
USTRUCT(BlueprintType)
struct FMTCatSkillDesc
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = true))
	FText Passive;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = true))
	FText Skill1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = true))
	FText Skill2;
};

/** 고양이별 스킬 설명 데이터 — 에디터에서 편집하고 HUD 위젯이 참조 (코드 하드코딩 대체). */
UCLASS(BlueprintType)
class MEOWTRACTIVE_API UMTSkillDescData : public UDataAsset
{
	GENERATED_BODY()

public:
	// 고양이 종류 → 설명 묶음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MT|SkillInfo")
	TMap<EMTCatType, FMTCatSkillDesc> Descriptions;
};
