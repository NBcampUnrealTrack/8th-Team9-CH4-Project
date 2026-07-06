// MTGameState.h
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Item/MTItemTypes.h"
#include "MTGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerScoresUpdated);

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

    // 내림차순 정렬된 랭킹 반환
    UFUNCTION(BlueprintPure)
    TArray<FPlayerScore> GetSortedPlayerScores() const;

protected:
    UPROPERTY(ReplicatedUsing = OnRep_PlayerScores)
    TArray<FPlayerScore> PlayerScores;

    UFUNCTION()
    void OnRep_PlayerScores();

	// 게임에 존재하는 모든 아이템 데이터. BP_MTGameState 디테일 창에서 채워 넣기.
	UPROPERTY(EditDefaultsOnly, Category = "Item")
	TArray<UMTItemData*> AllItemData;

	// AllItemData로부터 BeginPlay 시점에 구축되는 조회용 맵. 자바의 HashMap과 동일한 역할.
	TMap<EMTItemType, UMTItemData*> ItemRegistry;

};
