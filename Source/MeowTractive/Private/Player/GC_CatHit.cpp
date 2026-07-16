// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/GC_CatHit.h"
#include "Kismet/GameplayStatics.h"

UGC_CatHit::UGC_CatHit()
{
	// 태그(GameplayCue.Cat.Hit)는 BP 에셋에서 지정한다.
	// C++ 생성자에서 지정하면 부모/자식 태그가 같아져, 저장 시 엔진의
	// DeriveGameplayCueTagFromClass가 GameplayCueName(레지스트리 검색키)을 None으로
	// 기록 → 큐 매니저 스캔에서 누락돼 큐가 영영 실행되지 않는다.
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

