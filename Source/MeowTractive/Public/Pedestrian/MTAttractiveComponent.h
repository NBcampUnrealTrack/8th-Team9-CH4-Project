#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "MTAttractiveComponent.generated.h"

class APlayerState;
class UMTAttractiveComponent;

USTRUCT(BlueprintType)
struct FMTAttractiveAmountEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Attractive")
	TObjectPtr<APlayerState> PlayerState = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Attractive")
	float AttractiveAmount = 0.f;

	// 서버 전용: 이 시각 전까지 해당 플레이어의 매료도 감소를 정지한다.
	double RegenResumeTimeSeconds = 0.0;
};

USTRUCT()
struct FMTAttractiveAmountContainer : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMTAttractiveAmountEntry> Entries;

	// FastArray의 변경된 항목만 네트워크로 직렬화한다.
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FastArrayDeltaSerialize<FMTAttractiveAmountEntry, FMTAttractiveAmountContainer>(
			Entries,
			DeltaParams,
			*this);
	}

	void SetOwner(UMTAttractiveComponent* InOwner);
	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);

private:
	TWeakObjectPtr<UMTAttractiveComponent> Owner;
};

template<>
struct TStructOpsTypeTraits<FMTAttractiveAmountContainer>
	: public TStructOpsTypeTraitsBase2<FMTAttractiveAmountContainer>
{
	enum
	{
		WithNetDeltaSerializer = true
	};
};

/**
 * 행인 한 명에 대한 플레이어별 매료 수치를 서버 권위로 관리한다.
 * 클라이언트에는 FFastArraySerializer를 통해 변경된 항목만 복제한다.
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
	void DecreaseEligibleAttractiveAmounts(float Amount);
	void ResetAllAttractiveAmounts();
	void ResetOtherAttractiveAmounts(APlayerState* KeptPlayerState);

	UFUNCTION(BlueprintPure, Category = "Attractive")
	float GetAttractiveAmount(APlayerState* PlayerState) const;

	UFUNCTION(BlueprintPure, Category = "Attractive")
	float GetHighestAttractiveAmount() const;

	UFUNCTION(BlueprintPure, Category = "Attractive")
	float GetHighestAttractiveAmountExcluding(APlayerState* ExcludedPlayerState) const;

	UFUNCTION(BlueprintPure, Category = "Attractive")
	APlayerState* GetLeadingPlayer() const;

	UFUNCTION(BlueprintPure, Category = "Attractive")
	APlayerState* GetLeadingPlayerExcluding(APlayerState* ExcludedPlayerState) const;

	UFUNCTION(BlueprintPure, Category = "Attractive")
	float GetMaxAttractiveAmount() const { return MaxAttractiveAmount; }

	UFUNCTION(BlueprintPure, Category = "Attractive")
	TArray<FMTAttractiveAmountEntry> GetAttractiveAmounts() const { return AttractiveAmounts.Entries; }

private:
	friend struct FMTAttractiveAmountContainer;

	UPROPERTY(Replicated)
	FMTAttractiveAmountContainer AttractiveAmounts;

	UPROPERTY(EditDefaultsOnly, Category = "Attractive", meta = (ClampMin = "1.0"))
	float MaxAttractiveAmount = 100.f;

	UPROPERTY(EditDefaultsOnly, Category = "Attractive|Regen", meta = (ClampMin = "0.0"))
	float RegenDelayAfterInteraction = 7.f;

	TWeakObjectPtr<APlayerState> LastAttacker;

	bool HasServerAuthority() const;
	FMTAttractiveAmountEntry* FindEntry(APlayerState* PlayerState);
	const FMTAttractiveAmountEntry* FindEntry(APlayerState* PlayerState) const;
	void NotifyAmountsChanged();
	void HandleReplicatedAmountsChanged();
};
