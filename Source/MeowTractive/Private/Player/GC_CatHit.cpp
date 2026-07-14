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
		// 콤보 인덱스
		int32 ComboIndex = FMath::RoundToInt(Parameters.RawMagnitude);

		// 배열의 최대 크기를 넘지 못하도록 함
		int32 SafeIndex = FMath::Clamp(ComboIndex, 0, HitSounds.Num() - 1);

		// 맞은 위치에서 해당 인덱스의 소리를 재생
		UGameplayStatics::PlaySoundAtLocation(MyTarget, HitSounds[SafeIndex], MyTarget->GetActorLocation());
	}

	return Super::OnExecute_Implementation(MyTarget, Parameters);
}

