// MTGameState.h
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Item/MTItemTypes.h"
#include "MTGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerScoresUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchTimeUpdated, int32, RemainingSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMTOnMatchStateChanged);

class UMTItemData;

USTRUCT(BlueprintType)
struct FPlayerScore
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<APlayerState> PlayerState = nullptr;

    UPROPERTY(BlueprintReadOnly)
    int32 AttractedCount = 0;
};

UCLASS()
class MEOWTRACTIVE_API AMTGameState : public AGameState
{
    GENERATED_BODY()

public:
	virtual void PostInitializeComponents() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void AddPlayerState(APlayerState* PlayerState) override;
    virtual void RemovePlayerState(APlayerState* PlayerState) override;


    // 서버에서 호출
    void AddAttractedCount(APlayerState* TargetPlayerState);

	// 매료도가 떨어지면 카운트 내리기
    void RemoveAttractedCount(APlayerState* TargetPlayerState);

    UFUNCTION(BlueprintPure)
    int32 GetAttractedCount(APlayerState* TargetPlayerState) const;

	UFUNCTION(BlueprintPure, Category = "Item")
	UMTItemData* GetItemData(EMTItemType Type) const;

	// 클라이언트 위젯이 바인딩할 델리게이트
	UPROPERTY(BlueprintAssignable)
    FOnPlayerScoresUpdated OnPlayerScoresUpdated;

	// 매치 남은 시간 변경 (HUD가 구독)
	UPROPERTY(BlueprintAssignable)
	FOnMatchTimeUpdated OnMatchTimeUpdated;

	// 매치 상태(MatchState) 변경 — 서버·클라 공통 발동 (HUD 로딩 오버레이 등이 구독)
	UPROPERTY(BlueprintAssignable)
	FMTOnMatchStateChanged OnMatchStateChanged;

	// 서버: MatchGameMode가 1초마다 호출 → 복제 + 방송
	void SetMatchRemainingTime(int32 NewSeconds);

	UFUNCTION(BlueprintPure)
	int32 GetMatchRemainingTime() const { return MatchRemainingTime; }

    // 내림차순 정렬된 랭킹 반환
    UFUNCTION(BlueprintPure)
    TArray<FPlayerScore> GetSortedPlayerScores() const;

protected:
	virtual void OnRep_MatchState() override;

    UPROPERTY(ReplicatedUsing = OnRep_PlayerScores)
    TArray<FPlayerScore> PlayerScores;

    UFUNCTION()
    void OnRep_PlayerScores();

	// 매치 남은 시간 (초). 서버가 갱신, 클라는 OnRep으로 수신
	UPROPERTY(ReplicatedUsing = OnRep_MatchRemainingTime)
	int32 MatchRemainingTime = 0;

	UFUNCTION()
	void OnRep_MatchRemainingTime();

	// 게임에 존재하는 모든 아이템 데이터. BP_MTGameState 디테일 창에서 채워 넣기.
	UPROPERTY(EditDefaultsOnly, Category = "Item")
	TArray<UMTItemData*> AllItemData;

	// AllItemData로부터 BeginPlay 시점에 구축되는 조회용 맵. 자바의 HashMap과 동일한 역할.
	TMap<EMTItemType, UMTItemData*> ItemRegistry;

};
