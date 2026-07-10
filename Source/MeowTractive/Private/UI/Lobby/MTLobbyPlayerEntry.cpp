#include "UI/Lobby/MTLobbyPlayerEntry.h"
#include "UI/Lobby/MTLobbyHUDWidget.h"
#include "Player/MTPlayerState.h"
#include "Online/MTOnlineUtils.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Engine/Texture2D.h"
#include "TimerManager.h"

void UMTLobbyPlayerEntry::Setup(AMTPlayerState* PS, UMTLobbyHUDWidget* Owner)
{
	SlotPS = PS;
	OwnerHUD = Owner;

	RefreshVisuals();

	// 아바타 로딩 전엔 디자이너 기본 브러시(회색 고양이 실루엣) 유지 → 스팀 도착 시 교체
	AvatarRetries = 0;
	TryLoadAvatar();
}

void UMTLobbyPlayerEntry::RefreshVisuals()
{
	const bool bReady = SlotPS && SlotPS->IsReady();

	// Ready 테두리 색 (알파 0↔1)
	if (ReadyBorder)
	{
		ReadyBorder->SetBrushColor(bReady ? ReadyBorderColor : NotReadyBorderColor);
	}
	if (ReadyCheck)
	{
		ReadyCheck->SetVisibility(bReady ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UMTLobbyPlayerEntry::TryLoadAvatar()
{
	if (!SteamProfile || !OwnerHUD || !SlotPS)
	{
		return;
	}

	if (UTexture2D* Avatar = OwnerHUD->GetAvatar(SlotPS))
	{
		SteamProfile->SetBrushFromTexture(Avatar, false);   // 실루엣 → 스팀 아바타로 교체
		return;
	}

	// 아직 로딩 전 → 몇 번 재시도 (스팀 정보 도착 대기). Null OSS면 그냥 기본 아이콘 유지.
	if (++AvatarRetries <= 6)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(AvatarRetryTimer, this, &UMTLobbyPlayerEntry::TryLoadAvatar, 0.5f, false);
		}
	}
}

void UMTLobbyPlayerEntry::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AvatarRetryTimer);
	}
	Super::NativeDestruct();
}
