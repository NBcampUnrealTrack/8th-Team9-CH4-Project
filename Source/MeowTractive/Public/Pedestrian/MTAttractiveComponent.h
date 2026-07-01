#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MTAttractiveComponent.generated.h"

class APlayerState;

USTRUCT(BlueprintType)
struct FMTAttractiveAmountEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Attractive")
	TObjectPtr<APlayerState> PlayerState = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Attractive")
	float AttractiveAmount = 0.f;
};

/**
 * 행인 한 명에 대한 플레이어별 매료 수치를 관리한다.
 * 서버가 값을 변경하고 전체 배열을 클라이언트에 복제한다.
 */
UCLASS(ClassGroup = (MeowTractive), meta = (BlueprintSpawnableComponent))
class MEOWTRACTIVE_API UMTAttractiveComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMTAttractiveComponent();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void SynchronizePlayers();
	void AddPlayerState(APlayerState* PlayerState);
	void RemovePlayerState(APlayerState* PlayerState);

	float AddAttractiveAmount(APlayerState* PlayerState, float Amount);
	void DecreaseAllAttractiveAmounts(float Amount);
	void ResetAllAttractiveAmounts();

	UFUNCTION(BlueprintPure, Category = "Attractive")
	float GetAttractiveAmount(APlayerState* PlayerState) const;

	UFUNCTION(BlueprintPure, Category = "Attractive")
	float GetHighestAttractiveAmount() const;

	UFUNCTION(BlueprintPure, Category = "Attractive")
	float GetHighestAttractiveAmountExcluding(APlayerState* ExcludedPlayerState) const;

	UFUNCTION(BlueprintPure, Category = "Attractive")
	APlayerState* GetLeadingPlayer() const;

	UFUNCTION(BlueprintPure, Category = "Attractive")
	float GetMaxAttractiveAmount() const { return MaxAttractiveAmount; }

	UFUNCTION(BlueprintPure, Category = "Attractive")
	TArray<FMTAttractiveAmountEntry> GetAttractiveAmounts() const { return AttractiveAmounts; }

private:
	UPROPERTY(ReplicatedUsing = OnRep_AttractiveAmounts)
	TArray<FMTAttractiveAmountEntry> AttractiveAmounts;

	UPROPERTY(EditDefaultsOnly, Category = "Attractive", meta = (ClampMin = "1.0"))
	float MaxAttractiveAmount = 100.f;

	TWeakObjectPtr<APlayerState> LastAttacker;

	FMTAttractiveAmountEntry* FindEntry(APlayerState* PlayerState);
	const FMTAttractiveAmountEntry* FindEntry(APlayerState* PlayerState) const;
	void NotifyAmountsChanged();

	UFUNCTION()
	void OnRep_AttractiveAmounts();
};
