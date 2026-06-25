#include "Player/MTPlayerState.h"
#include "Net/UnrealNetwork.h"

AMTPlayerState::AMTPlayerState()
{
	SetReplicates(true);
}

void AMTPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// 개인전: 선택한 고양이는 소유자(+서버)에게만 복제 → 상대는 알 수 없음(카운터픽 방지)
	DOREPLIFETIME_CONDITION(AMTPlayerState, SelectedCatType, COND_OwnerOnly);
	DOREPLIFETIME(AMTPlayerState, bIsReady);
	DOREPLIFETIME(AMTPlayerState, PlayerSlot);
	DOREPLIFETIME(AMTPlayerState, TeamId);
	DOREPLIFETIME(AMTPlayerState, bIsLoaded);
	DOREPLIFETIME(AMTPlayerState, bIsHost);
}

void AMTPlayerState::SetSelectedCat(EMTCatType NewCat)
{
	if (!HasAuthority() || SelectedCatType == NewCat)
	{
		return;
	}
	SelectedCatType = NewCat;
	BroadcastChanged();
}

void AMTPlayerState::SetReady(bool bNewReady)
{
	if (!HasAuthority() || bIsReady == bNewReady)
	{
		return;
	}
	bIsReady = bNewReady;
	BroadcastChanged();
}

void AMTPlayerState::SetPlayerSlot(int32 NewSlot)
{
	if (!HasAuthority())
	{
		return;
	}
	PlayerSlot = NewSlot;
	BroadcastChanged();
}

void AMTPlayerState::SetLoaded(bool bNewLoaded)
{
	if (!HasAuthority())
	{
		return;
	}
	bIsLoaded = bNewLoaded;
}

void AMTPlayerState::SetHost(bool bNewHost)
{
	if (!HasAuthority())
	{
		return;
	}
	bIsHost = bNewHost;
	BroadcastChanged();
}

void AMTPlayerState::OnRep_LobbyState()
{
	BroadcastChanged();
}

void AMTPlayerState::BroadcastChanged()
{
	OnLobbyStateChanged.Broadcast();
}

void AMTPlayerState::SetTeamColor(FLinearColor NewColor)
{
	if (!HasAuthority()) return;
    TeamColor = NewColor;
}

// seamless travel 시 선택 정보 유지. bIsReady/bIsLoaded는 새 레벨에서 다시 받음.
void AMTPlayerState::CopyProperties(APlayerState* NewPlayerState)
{
	Super::CopyProperties(NewPlayerState);
	if (AMTPlayerState* NewMTPS = Cast<AMTPlayerState>(NewPlayerState))
	{
		NewMTPS->SelectedCatType = SelectedCatType;
		NewMTPS->PlayerSlot = PlayerSlot;
		NewMTPS->TeamId = TeamId;
		NewMTPS->bIsHost = bIsHost;
	}
}
