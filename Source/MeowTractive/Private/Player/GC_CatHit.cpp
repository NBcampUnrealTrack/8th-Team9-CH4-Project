// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/GC_CatHit.h"
#include "Kismet/GameplayStatics.h"

UGC_CatHit::UGC_CatHit()
{
	// 스피커가 반응할 태그
	GameplayCueTag = FGameplayTag::RequestGameplayTag(FName("GameplayCue.Cat.Hit"));
}

bool UGC_CatHit::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	// 사운드 배열이 비어있지 않고, 맞은 대상이 존재할 때만 실행
	if (HitSounds.Num() > 0 && MyTarget != nullptr)
	{
		// 매번 랜덤 재생
		int32 RandomIndex = FMath::RandRange(0, HitSounds.Num() - 1);

		// 맞은 위치에서 해당 인덱스의 소리를 재생
		UGameplayStatics::PlaySoundAtLocation(MyTarget, HitSounds[RandomIndex], MyTarget->GetActorLocation());
	}

	return Super::OnExecute_Implementation(MyTarget, Parameters);
}

