// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "GC_CatHit.generated.h"

/**
 *
 */
UCLASS()
class MEOWTRACTIVE_API UGC_CatHit : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	UGC_CatHit();

	// 타격음 재생
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

	// 사운드 파일 배열
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	TArray<USoundBase*> HitSounds;
};

