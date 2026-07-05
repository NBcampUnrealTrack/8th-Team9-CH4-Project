#include "Pedestrian/MTAttractiveComponent.h"

#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Pedestrian/MTPedestrianBase.h"

// FastArray가 변경 알림을 전달할 Component를 설정한다.
void FMTAttractiveAmountContainer::SetOwner(UMTAttractiveComponent* InOwner)
{
	Owner = InOwner;
}

// 클라이언트가 FastArray의 추가·수정·삭제를 모두 반영한 뒤 UI 갱신을 요청한다.
void FMTAttractiveAmountContainer::PostReplicatedReceive(
	const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
	if (Owner.IsValid())
	{
		Owner->HandleReplicatedAmountsChanged();
	}
}

// Component의 Tick과 FastArray 복제를 초기화한다.
UMTAttractiveComponent::UMTAttractiveComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	AttractiveAmounts.SetOwner(this);
}

// 서버에서 현재 참가 플레이어 목록을 최초 동기화한다.
void UMTAttractiveComponent::BeginPlay()
{
	Super::BeginPlay();

	AttractiveAmounts.SetOwner(this);
	if (HasServerAuthority())
	{
		SynchronizePlayers();
	}
}

// FastArray Container를 Component의 복제 속성으로 등록한다.
void UMTAttractiveComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UMTAttractiveComponent, AttractiveAmounts);
}

// 현재 GameState의 PlayerArray와 매료도 항목 목록을 일치시킨다.
void UMTAttractiveComponent::SynchronizePlayers()
{
	AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!HasServerAuthority() || !GameState)
	{
		return;
	}

	bool bChanged = false;
	const int32 RemovedCount = AttractiveAmounts.Entries.RemoveAll(
		[GameState](const FMTAttractiveAmountEntry& Entry)
		{
			return !IsValid(Entry.PlayerState)
				|| !GameState->PlayerArray.Contains(Entry.PlayerState);
		});

	if (RemovedCount > 0)
	{
		AttractiveAmounts.MarkArrayDirty();
		bChanged = true;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (IsValid(PlayerState) && !FindEntry(PlayerState))
		{
			FMTAttractiveAmountEntry& Entry = AttractiveAmounts.Entries.AddDefaulted_GetRef();
			Entry.PlayerState = PlayerState;
			Entry.AttractiveAmount = 0.f;
			Entry.RegenResumeTimeSeconds = 0.0;
			AttractiveAmounts.MarkItemDirty(Entry);
			bChanged = true;
		}
	}

	if (bChanged)
	{
		NotifyAmountsChanged();
	}
}

// 서버에서 새 플레이어의 매료도 항목을 추가한다.
void UMTAttractiveComponent::AddPlayerState(APlayerState* PlayerState)
{
	if (!HasServerAuthority() || !IsValid(PlayerState) || FindEntry(PlayerState))
	{
		return;
	}

	FMTAttractiveAmountEntry& Entry = AttractiveAmounts.Entries.AddDefaulted_GetRef();
	Entry.PlayerState = PlayerState;
	Entry.AttractiveAmount = 0.f;
	Entry.RegenResumeTimeSeconds = 0.0;
	AttractiveAmounts.MarkItemDirty(Entry);
	NotifyAmountsChanged();
}

// 서버에서 퇴장한 플레이어의 매료도 항목을 제거한다.
void UMTAttractiveComponent::RemovePlayerState(APlayerState* PlayerState)
{
	if (!HasServerAuthority() || !PlayerState)
	{
		return;
	}

	const int32 RemovedCount = AttractiveAmounts.Entries.RemoveAll(
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
		AttractiveAmounts.MarkArrayDirty();
		NotifyAmountsChanged();
	}
}

// 서버에서 특정 플레이어의 매료도를 올리고 해당 플레이어의 Regen 대기시간만 갱신한다.
float UMTAttractiveComponent::AddAttractiveAmount(APlayerState* PlayerState, float Amount)
{
	if (!HasServerAuthority() || !IsValid(PlayerState) || Amount <= 0.f)
	{
		return GetAttractiveAmount(PlayerState);
	}

	FMTAttractiveAmountEntry* Entry = FindEntry(PlayerState);
	if (!Entry)
	{
		FMTAttractiveAmountEntry& NewEntry = AttractiveAmounts.Entries.AddDefaulted_GetRef();
		NewEntry.PlayerState = PlayerState;
		NewEntry.AttractiveAmount = 0.f;
		NewEntry.RegenResumeTimeSeconds = 0.0;
		AttractiveAmounts.MarkItemDirty(NewEntry);
		Entry = &NewEntry;
	}

	const float PreviousAmount = Entry->AttractiveAmount;
	Entry->AttractiveAmount = FMath::Clamp(
		PreviousAmount + Amount,
		0.f,
		MaxAttractiveAmount);
	Entry->RegenResumeTimeSeconds =
		(GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) + RegenDelayAfterInteraction;
	LastAttacker = PlayerState;

	if (!FMath::IsNearlyEqual(PreviousAmount, Entry->AttractiveAmount))
	{
		AttractiveAmounts.MarkItemDirty(*Entry);
		NotifyAmountsChanged();
	}

	return Entry->AttractiveAmount;
}

// 서버에서 상호작용 대기시간이 끝난 미완료 매료도만 주기적으로 감소시킨다.
void UMTAttractiveComponent::DecreaseEligibleAttractiveAmounts(float Amount)
{
	if (!HasServerAuthority() || Amount <= 0.f)
	{
		return;
	}

	const double CurrentTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	bool bChanged = false;

	for (FMTAttractiveAmountEntry& Entry : AttractiveAmounts.Entries)
	{
		// 매료 완료된 수치는 Regen 대상에서 제외한다.
		if (Entry.AttractiveAmount >= MaxAttractiveAmount)
		{
			continue;
		}

		const bool bCanDecrease =
			Entry.AttractiveAmount > 0.f
			&& CurrentTimeSeconds >= Entry.RegenResumeTimeSeconds;
		if (!bCanDecrease)
		{
			continue;
		}

		const float NewAmount = FMath::Max(0.f, Entry.AttractiveAmount - Amount);
		if (FMath::IsNearlyEqual(NewAmount, Entry.AttractiveAmount))
		{
			continue;
		}

		Entry.AttractiveAmount = NewAmount;
		if (Entry.AttractiveAmount <= 0.f)
		{
			Entry.RegenResumeTimeSeconds = 0.0;
		}
		AttractiveAmounts.MarkItemDirty(Entry);
		bChanged = true;
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

// 서버에서 모든 플레이어의 매료도와 Regen 대기시간을 초기화한다.
void UMTAttractiveComponent::ResetAllAttractiveAmounts()
{
	if (!HasServerAuthority())
	{
		return;
	}

	bool bChanged = false;
	for (FMTAttractiveAmountEntry& Entry : AttractiveAmounts.Entries)
	{
		Entry.RegenResumeTimeSeconds = 0.0;
		if (Entry.AttractiveAmount <= 0.f)
		{
			continue;
		}

		Entry.AttractiveAmount = 0.f;
		AttractiveAmounts.MarkItemDirty(Entry);
		bChanged = true;
	}

	LastAttacker.Reset();
	if (bChanged)
	{
		NotifyAmountsChanged();
	}
}

// 서버에서 매료 완료 플레이어를 제외한 모든 플레이어 수치와 대기시간을 초기화한다.
void UMTAttractiveComponent::ResetOtherAttractiveAmounts(APlayerState* KeptPlayerState)
{
	if (!HasServerAuthority() || !IsValid(KeptPlayerState))
	{
		return;
	}

	bool bChanged = false;
	for (FMTAttractiveAmountEntry& Entry : AttractiveAmounts.Entries)
	{
		if (Entry.PlayerState == KeptPlayerState)
		{
			continue;
		}

		Entry.RegenResumeTimeSeconds = 0.0;
		if (Entry.AttractiveAmount <= 0.f)
		{
			continue;
		}

		Entry.AttractiveAmount = 0.f;
		AttractiveAmounts.MarkItemDirty(Entry);
		bChanged = true;
	}

	LastAttacker = KeptPlayerState;
	if (bChanged)
	{
		NotifyAmountsChanged();
	}
}

// 특정 플레이어의 현재 매료도를 반환한다.
float UMTAttractiveComponent::GetAttractiveAmount(APlayerState* PlayerState) const
{
	const FMTAttractiveAmountEntry* Entry = FindEntry(PlayerState);
	return Entry ? Entry->AttractiveAmount : 0.f;
}

// 유효한 플레이어 항목 중 가장 높은 매료도를 반환한다.
float UMTAttractiveComponent::GetHighestAttractiveAmount() const
{
	float HighestAmount = 0.f;
	for (const FMTAttractiveAmountEntry& Entry : AttractiveAmounts.Entries)
	{
		if (IsValid(Entry.PlayerState))
		{
			HighestAmount = FMath::Max(HighestAmount, Entry.AttractiveAmount);
		}
	}
	return HighestAmount;
}

// 지정한 플레이어를 제외한 가장 높은 매료도를 반환한다.
float UMTAttractiveComponent::GetHighestAttractiveAmountExcluding(
	APlayerState* ExcludedPlayerState) const
{
	float HighestAmount = 0.f;
	for (const FMTAttractiveAmountEntry& Entry : AttractiveAmounts.Entries)
	{
		if (IsValid(Entry.PlayerState) && Entry.PlayerState != ExcludedPlayerState)
		{
			HighestAmount = FMath::Max(HighestAmount, Entry.AttractiveAmount);
		}
	}
	return HighestAmount;
}

// 동점이면 마지막 공격자를 우선해 현재 선두 플레이어를 반환한다.
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

	for (const FMTAttractiveAmountEntry& Entry : AttractiveAmounts.Entries)
	{
		if (IsValid(Entry.PlayerState)
			&& FMath::IsNearlyEqual(Entry.AttractiveAmount, HighestAmount, 0.01f))
		{
			return Entry.PlayerState;
		}
	}
	return nullptr;
}

// 지정한 플레이어를 제외하고 동점 우선순위를 적용한 선두 플레이어를 반환한다.
APlayerState* UMTAttractiveComponent::GetLeadingPlayerExcluding(
	APlayerState* ExcludedPlayerState) const
{
	const float HighestAmount = GetHighestAttractiveAmountExcluding(ExcludedPlayerState);
	if (HighestAmount <= 0.f)
	{
		return nullptr;
	}

	if (LastAttacker.IsValid() && LastAttacker.Get() != ExcludedPlayerState)
	{
		const FMTAttractiveAmountEntry* LastAttackerEntry = FindEntry(LastAttacker.Get());
		if (LastAttackerEntry
			&& FMath::IsNearlyEqual(LastAttackerEntry->AttractiveAmount, HighestAmount, 0.01f))
		{
			return LastAttacker.Get();
		}
	}

	for (const FMTAttractiveAmountEntry& Entry : AttractiveAmounts.Entries)
	{
		if (IsValid(Entry.PlayerState)
			&& Entry.PlayerState != ExcludedPlayerState
			&& FMath::IsNearlyEqual(Entry.AttractiveAmount, HighestAmount, 0.01f))
		{
			return Entry.PlayerState;
		}
	}
	return nullptr;
}

// 현재 실행 주체가 매료도 데이터를 변경할 수 있는 서버인지 확인한다.
bool UMTAttractiveComponent::HasServerAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor && OwnerActor->HasAuthority();
}

// 지정한 플레이어의 수정 가능한 FastArray 항목을 찾는다.
FMTAttractiveAmountEntry* UMTAttractiveComponent::FindEntry(APlayerState* PlayerState)
{
	return AttractiveAmounts.Entries.FindByPredicate(
		[PlayerState](const FMTAttractiveAmountEntry& Entry)
		{
			return Entry.PlayerState == PlayerState;
		});
}

// 지정한 플레이어의 읽기 전용 FastArray 항목을 찾는다.
const FMTAttractiveAmountEntry* UMTAttractiveComponent::FindEntry(
	APlayerState* PlayerState) const
{
	return AttractiveAmounts.Entries.FindByPredicate(
		[PlayerState](const FMTAttractiveAmountEntry& Entry)
		{
			return Entry.PlayerState == PlayerState;
		});
}

// 서버 변경을 즉시 네트워크 갱신 대상으로 만들고 로컬 표시를 갱신한다.
void UMTAttractiveComponent::NotifyAmountsChanged()
{
	if (!HasServerAuthority())
	{
		return;
	}

	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
	HandleReplicatedAmountsChanged();
}

// 서버 또는 FastArray 복제 수신 후 행인의 UI와 선두 상태를 갱신한다.
void UMTAttractiveComponent::HandleReplicatedAmountsChanged()
{
	if (AMTPedestrianBase* Pedestrian = Cast<AMTPedestrianBase>(GetOwner()))
	{
		Pedestrian->HandleAttractiveAmountsChanged();
	}
}
