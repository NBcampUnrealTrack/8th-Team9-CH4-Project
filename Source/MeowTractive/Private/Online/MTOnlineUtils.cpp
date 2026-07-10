#include "Online/MTOnlineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"

#if WITH_STEAM_AVATAR
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END
#endif

FString UMTOnlineUtils::GetPersonaName(const APlayerState* PlayerState)
{
	return PlayerState ? PlayerState->GetPlayerName() : FString();
}

UTexture2D* UMTOnlineUtils::GetSteamAvatar(const APlayerState* PlayerState)
{
#if WITH_STEAM_AVATAR
	if (!PlayerState || !SteamFriends() || !SteamUtils())
	{
		return nullptr;   // Null(LAN)/스팀 미실행 → 아바타 없음
	}

	const FUniqueNetIdRepl& NetId = PlayerState->GetUniqueId();
	if (!NetId.IsValid())
	{
		return nullptr;
	}

	const uint64 SteamId64 = FCString::Strtoui64(*NetId->ToString(), nullptr, 10);
	if (SteamId64 == 0)
	{
		return nullptr;
	}
	const CSteamID SteamID(SteamId64);

	const int AvatarHandle = SteamFriends()->GetLargeFriendAvatar(SteamID);
	if (AvatarHandle <= 0)
	{
		// 아직 로드 안 됨 → 스팀에 정보 요청 트리거 후 다음 호출에서 반환
		SteamFriends()->RequestUserInformation(SteamID, false);
		return nullptr;
	}

	uint32 W = 0, H = 0;
	if (!SteamUtils()->GetImageSize(AvatarHandle, &W, &H) || W == 0 || H == 0)
	{
		return nullptr;
	}

	TArray<uint8> Buffer;
	Buffer.SetNumUninitialized(W * H * 4);
	if (!SteamUtils()->GetImageRGBA(AvatarHandle, Buffer.GetData(), Buffer.Num()))
	{
		return nullptr;
	}

	// Steam은 RGBA 순서 → PF_R8G8B8A8로 그대로 복사 (스왑 불필요)
	UTexture2D* Avatar = UTexture2D::CreateTransient(W, H, PF_R8G8B8A8);
	if (!Avatar)
	{
		return nullptr;
	}
	if (FTexturePlatformData* PData = Avatar->GetPlatformData())
	{
		void* Dest = PData->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(Dest, Buffer.GetData(), Buffer.Num());
		PData->Mips[0].BulkData.Unlock();
	}
	Avatar->UpdateResource();
	return Avatar;
#else
	return nullptr;
#endif
}
