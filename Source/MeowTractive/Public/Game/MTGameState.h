// MTGameState.h
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "MTGameState.generated.h"

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
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // 서버에서 호출
    void AddAttractedCount(APlayerState* TargetPlayerState);

	// 매료도가 떨어지면 카운트 내리기
    void RemoveAttractedCount(APlayerState* TargetPlayerState);

    UFUNCTION(BlueprintPure)
    int32 GetAttractedCount(APlayerState* TargetPlayerState) const;

protected:
    UPROPERTY(ReplicatedUsing = OnRep_PlayerScores)
    TArray<FPlayerScore> PlayerScores;

    UFUNCTION()
    void OnRep_PlayerScores();
};
