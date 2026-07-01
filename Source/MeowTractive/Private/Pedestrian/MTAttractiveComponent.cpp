#include "Pedestrian/MTAttractiveComponent.h"

#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Pedestrian/MTPedestrianBase.h"

UMTAttractiveComponent::UMTAttractiveComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMTAttractiveComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		SynchronizePlayers();
	}
}

//네트워크 복제 설정
void UMTAttractiveComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UMTAttractiveComponent, AttractiveAmounts);
}

//매료도 목록 최신화, 없는 플레이어 제거
void UMTAttractiveComponent::SynchronizePlayers()
{
	AActor* Owner = GetOwner();
	AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!Owner || !Owner->HasAuthority() || !GameState)
	{
		return;
	}

	bool bChanged = AttractiveAmounts.RemoveAll(
		[GameState](const FMTAttractiveAmountEntry& Entry)
		{
			return !IsValid(Entry.PlayerState) || !GameState->PlayerArray.Contains(Entry.PlayerState);
		}) > 0;

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (IsValid(PlayerState) && !FindEntry(PlayerState))
		{
			FMTAttractiveAmountEntry& Entry = AttractiveAmounts.AddDefaulted_GetRef();
			Entry.PlayerState = PlayerState;
			Entry.AttractiveAmount = 0.f;
			bChanged = true;
		}
	}

	if (bChanged)
	{
		NotifyAmountsChanged();
	}
}

//매료도 목록에 플레이어 추가
void UMTAttractiveComponent::AddPlayerState(APlayerState* PlayerState)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !IsValid(PlayerState) || FindEntry(PlayerState))
	{
		return;
	}

	FMTAttractiveAmountEntry& Entry = AttractiveAmounts.AddDefaulted_GetRef();
	Entry.PlayerState = PlayerState;
	Entry.AttractiveAmount = 0.f;
	NotifyAmountsChanged();
}

//매료도 목록에 플레이어 제거
void UMTAttractiveComponent::RemovePlayerState(APlayerState* PlayerState)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !PlayerState)
	{
		return;
	}

	const int32 RemovedCount = AttractiveAmounts.RemoveAll(
		[PlayerState](const FMTAttractiveAmountEntry& Entry)
		{
			return Entry.PlayerState == PlayerState;
		});

	if (LastAttacker.Get() == PlayerState)
	{
		LastAttacker.Reset();
	}
	if (RemovedCount > 0)
	{
		NotifyAmountsChanged();
	}
}

//매료도 추가
float UMTAttractiveComponent::AddAttractiveAmount(APlayerState* PlayerState, float Amount)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !IsValid(PlayerState) || Amount <= 0.f)
	{
		return GetAttractiveAmount(PlayerState);
	}

	AddPlayerState(PlayerState);
	FMTAttractiveAmountEntry* Entry = FindEntry(PlayerState);
	if (!Entry)
	{
		return 0.f;
	}

	Entry->AttractiveAmount = FMath::Clamp(
		Entry->AttractiveAmount + Amount,
		0.f,
		MaxAttractiveAmount);
	LastAttacker = PlayerState;
	NotifyAmountsChanged();
	return Entry->AttractiveAmount;
}

//모든 플레이어 매료도 감소
void UMTAttractiveComponent::DecreaseAllAttractiveAmounts(float Amount)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || Amount <= 0.f)
	{
		return;
	}

	bool bChanged = false;
	for (FMTAttractiveAmountEntry& Entry : AttractiveAmounts)
	{
		const float NewAmount = FMath::Max(0.f, Entry.AttractiveAmount - Amount);
		bChanged |= !FMath::IsNearlyEqual(NewAmount, Entry.AttractiveAmount);
		Entry.AttractiveAmount = NewAmount;
	}

	if (bChanged)
	{
		if (GetHighestAttractiveAmount() <= 0.f)
		{
			LastAttacker.Reset();
		}
		NotifyAmountsChanged();
	}
}

//모든 플레이어 매료도 0으로 초기화
void UMTAttractiveComponent::ResetAllAttractiveAmounts()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	for (FMTAttractiveAmountEntry& Entry : AttractiveAmounts)
	{
		Entry.AttractiveAmount = 0.f;
	}
	LastAttacker.Reset();
	NotifyAmountsChanged();
}

//현재 매료도 확인
float UMTAttractiveComponent::GetAttractiveAmount(APlayerState* PlayerState) const
{
	const FMTAttractiveAmountEntry* Entry = FindEntry(PlayerState);
	return Entry ? Entry->AttractiveAmount : 0.f;
}

//가장 높은 매료도 확인
float UMTAttractiveComponent::GetHighestAttractiveAmount() const
{
	float HighestAmount = 0.f;
	for (const FMTAttractiveAmountEntry& Entry : AttractiveAmounts)
	{
		if (IsValid(Entry.PlayerState))
		{
			HighestAmount = FMath::Max(HighestAmount, Entry.AttractiveAmount);
		}
	}
	return HighestAmount;
}

//특정 플레이어 제외 가장 높은 매료도 확인 (데미지 감쇄 계산용)
float UMTAttractiveComponent::GetHighestAttractiveAmountExcluding(APlayerState* ExcludedPlayerState) const
{
	float HighestAmount = 0.f;
	for (const FMTAttractiveAmountEntry& Entry : AttractiveAmounts)
	{
		if (IsValid(Entry.PlayerState) && Entry.PlayerState != ExcludedPlayerState)
		{
			HighestAmount = FMath::Max(HighestAmount, Entry.AttractiveAmount);
		}
	}
	return HighestAmount;
}

//가장 매료도 높은 플레이어 확인
APlayerState* UMTAttractiveComponent::GetLeadingPlayer() const
{
	const float HighestAmount = GetHighestAttractiveAmount();
	if (HighestAmount <= 0.f)
	{
		return nullptr;
	}

	if (LastAttacker.IsValid())
	{
		const FMTAttractiveAmountEntry* LastAttackerEntry = FindEntry(LastAttacker.Get());
		if (LastAttackerEntry
			&& FMath::IsNearlyEqual(LastAttackerEntry->AttractiveAmount, HighestAmount, 0.01f))
		{
			return LastAttacker.Get();
		}
	}

	for (const FMTAttractiveAmountEntry& Entry : AttractiveAmounts)
	{
		if (IsValid(Entry.PlayerState)
			&& FMath::IsNearlyEqual(Entry.AttractiveAmount, HighestAmount, 0.01f))
		{
			return Entry.PlayerState;
		}
	}
	return nullptr;
}

// 특정 플레이어의의 매료도로 등록된 요소를 목록에서 확인
FMTAttractiveAmountEntry* UMTAttractiveComponent::FindEntry(APlayerState* PlayerState)
{
	return AttractiveAmounts.FindByPredicate(
		[PlayerState](const FMTAttractiveAmountEntry& Entry)
		{
			return Entry.PlayerState == PlayerState;
		});
}

// 특정 플레이어의의 매료도로 등록된 요소를 목록에서 확인
const FMTAttractiveAmountEntry* UMTAttractiveComponent::FindEntry(APlayerState* PlayerState) const
{
	return AttractiveAmounts.FindByPredicate(
		[PlayerState](const FMTAttractiveAmountEntry& Entry)
		{
			return Entry.PlayerState == PlayerState;
		});
}

//변경사항 Replication 알림
void UMTAttractiveComponent::NotifyAmountsChanged()
{
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
	OnRep_AttractiveAmounts();
}

//
void UMTAttractiveComponent::OnRep_AttractiveAmounts()
{
	if (AMTPedestrianBase* Pedestrian = Cast<AMTPedestrianBase>(GetOwner()))
	{
		Pedestrian->HandleAttractiveAmountsChanged();
	}
}
