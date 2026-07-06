#include "Game/MTLobbyGameState.h"
#include "Net/UnrealNetwork.h"

void AMTLobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMTLobbyGameState, RoomName);
	DOREPLIFETIME(AMTLobbyGameState, RoomGameMode);
	DOREPLIFETIME(AMTLobbyGameState, RoomMap);
	DOREPLIFETIME(AMTLobbyGameState, bHasPassword);
}

void AMTLobbyGameState::SetRoomSettings(const FMTRoomSettings& NewSettings)
{
	if (!HasAuthority())
	{
		return;
	}
	RoomName = NewSettings.RoomName;
	RoomGameMode = NewSettings.GameMode;
	RoomMap = NewSettings.Map;
	bHasPassword = NewSettings.HasPassword();

	// 리슨 서버 호스트 UI도 즉시 갱신 (OnRep은 클라 전용)
	OnRoomSettingsChanged.Broadcast();
}

void AMTLobbyGameState::OnRep_RoomSettings()
{
	OnRoomSettingsChanged.Broadcast();
}
