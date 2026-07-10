#include "Online/MTOnlineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "Game/MTLog.h"

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
	if (!PlayerState)
	{
		return nullptr;
	}
	if (!SteamFriends() || !SteamUtils())
	{
		UE_LOG(LogMT, Warning, TEXT("[MTAvatar] SteamFriends/Utils null — SteamAPI 미초기화 (세션은 Steam OSS인지 확인)"));
		return nullptr;
	}

	const FUniqueNetIdRepl& NetId = PlayerState->GetUniqueId();
	if (!NetId.IsValid())
	{
		UE_LOG(LogMT, Warning, TEXT("[MTAvatar] NetId invalid — %s"), *PlayerState->GetPlayerName());
		return nullptr;
	}

	const uint64 SteamId64 = FCString::Strtoui64(*NetId->ToString(), nullptr, 10);
	if (SteamId64 == 0)
	{
		UE_LOG(LogMT, Warning, TEXT("[MTAvatar] SteamId64=0 — NetId=%s (스팀 ID 아님?)"), *NetId->ToString());
		return nullptr;
	}
	const CSteamID SteamID(SteamId64);

	const int AvatarHandle = SteamFriends()->GetLargeFriendAvatar(SteamID);
	if (AvatarHandle <= 0)
	{
		// 아직 로드 안 됨(-1)/없음(0) → 정보 요청 트리거 후 다음 호출에서 반환
		UE_LOG(LogMT, Warning, TEXT("[MTAvatar] AvatarHandle=%d (로딩중/없음) → RequestUserInformation, SteamId=%llu"), AvatarHandle, SteamId64);
		SteamFriends()->RequestUserInformation(SteamID, false);
		return nullptr;
	}

	uint32 W = 0, H = 0;
	if (!SteamUtils()->GetImageSize(AvatarHandle, &W, &H) || W == 0 || H == 0)
	{
		UE_LOG(LogMT, Warning, TEXT("[MTAvatar] GetImageSize 실패 (Handle=%d)"), AvatarHandle);
		return nullptr;
	}

	UE_LOG(LogMT, Log, TEXT("[MTAvatar] 아바타 로드 성공 %ux%u (SteamId=%llu)"), W, H, SteamId64);

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
	UE_LOG(LogMT, Warning, TEXT("[MTAvatar] WITH_STEAM_AVATAR=0 — 이 빌드는 스팀 아바타 비활성 (서버 타깃으로 패키징됐는지 확인)"));
	return nullptr;
#endif
}
