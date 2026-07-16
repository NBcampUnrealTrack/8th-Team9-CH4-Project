// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/InGame/MTMatchGameResultWidget.h"
#include "Game/MTGameState.h"
#include "Player/MTPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/AmbientSound.h"
#include "Components/AudioComponent.h"

void UMTMatchGameResultWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 매치→로비 seamless travel 시 이전 판 결과 위젯이 뷰포트에 남는 것 방지
    WorldTearDownHandle = FWorldDelegates::OnWorldBeginTearDown.AddUObject(
        this, &UMTMatchGameResultWidget::HandleWorldTearDown);
}

void UMTMatchGameResultWidget::NativeDestruct()
{
    FWorldDelegates::OnWorldBeginTearDown.Remove(WorldTearDownHandle);
    Super::NativeDestruct();
}

void UMTMatchGameResultWidget::HandleWorldTearDown(UWorld* World)
{
    if (World == GetWorld())
    {
        UE_LOG(LogTemp, Log, TEXT("[MTResult] 월드 종료 → 결과 위젯 제거: %s"), *GetName());
        RemoveFromParent();
    }
}

bool UMTMatchGameResultWidget::IsHost() const
{
    const APlayerController* PC = GetOwningPlayer();
    return PC && PC->HasAuthority();
}

void UMTMatchGameResultWidget::ShowResult()
{
    AMTGameState* GS = GetWorld()->GetGameState<AMTGameState>();
    if (!GS) return;

    // 중복 호출 진단용 — 4행 버그 재발 시 이 로그로 호출 횟수/인스턴스 확인
    UE_LOG(LogTemp, Log, TEXT("[MTResult] ShowResult 호출: widget=%s players=%d"),
        *GetName(), GS->PlayerArray.Num());

    // 점수 기반 결과 목록 생성
    TArray<FMTPlayerResult> Results;
    for (APlayerState* PS : GS->PlayerArray)
    {
        AMTPlayerState* MTPS = Cast<AMTPlayerState>(PS);
        if (!MTPS) continue;

        FMTPlayerResult Entry;
        Entry.PlayerName = PS->GetPlayerName();
        Entry.TeamColor = MTPS->GetTeamColor();
        Entry.AttractedCount = GS->GetAttractedCount(MTPS);
        Results.Add(Entry);
    }

    // 점수 내림차순 정렬
    Results.Sort([](const FMTPlayerResult& A, const FMTPlayerResult& B)
    {
        return A.AttractedCount > B.AttractedCount;
    });


	// 순위 부여 - 동점이면 같은 순위
	int32 CurrentRank = 1;
	for (int32 i = 0; i < Results.Num(); i++)
	{
		if (i > 0 && Results[i].AttractedCount < Results[i-1].AttractedCount)
		{
			CurrentRank = i + 1;
		}
		Results[i].Rank = CurrentRank;
	}

	OnResultReady(Results);

	// BGM 페이드아웃
	TArray<AActor*> BGMActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAmbientSound::StaticClass(), BGMActors);

	for (AActor* Actor : BGMActors)
	{
		if (AAmbientSound* BGM = Cast<AAmbientSound>(Actor))
		{
			if (UAudioComponent* AudioComp = BGM->GetAudioComponent())
			{
				AudioComp->FadeOut(1.0f, 0.1f); // 1초에 걸쳐 볼륨 0.1까지 감소
			}
		}
	}
}
