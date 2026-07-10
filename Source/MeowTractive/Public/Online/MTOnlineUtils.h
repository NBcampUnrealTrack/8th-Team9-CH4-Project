#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MTOnlineUtils.generated.h"

class APlayerState;
class UTexture2D;

/** 온라인 프로필 헬퍼 — 스팀 닉네임/아바타를 BP에서 사용. */
UCLASS()
class MEOWTRACTIVE_API UMTOnlineUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 스팀 페르소나 이름 (Steam OSS면 자동, 아니면 PlayerName)
	UFUNCTION(BlueprintPure, Category = "MT|Online")
	static FString GetPersonaName(const APlayerState* PlayerState);

	// 스팀 아바타를 UTexture2D로. Steam 미연동/미준비면 nullptr.
	// 아바타가 아직 로드 안 됐으면 첫 호출에서 nullptr → 잠시 후 재호출하면 반환됨.
	UFUNCTION(BlueprintCallable, Category = "MT|Online")
	static UTexture2D* GetSteamAvatar(const APlayerState* PlayerState);
};
