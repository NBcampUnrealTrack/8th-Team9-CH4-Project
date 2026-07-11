#include "Online/MTOnlineUtils.h"
#include "Player/MTPlayerState.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
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

void UMTOnlineUtils::ApplyFallbackPlayerName(APlayerState* PlayerState)
{
	if (!PlayerState || !PlayerState->HasAuthority())
	{
		return;
	}

	// Steam OSS면 페르소나 이름이 이미 유효 — 그대로 사용
	const IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
	if (OSS && OSS->GetSubsystemName() == STEAM_SUBSYSTEM)
	{
		return;
	}

	// 슬롯 우선, 미배정(PIE 직행 등)이면 접속 순서
	int32 Number = INDEX_NONE;
	if (const AMTPlayerState* MTPS = Cast<AMTPlayerState>(PlayerState))
	{
		Number = MTPS->GetPlayerSlot();
	}
	if (Number < 0)
	{
		const AGameStateBase* GS = PlayerState->GetWorld() ? PlayerState->GetWorld()->GetGameState() : nullptr;
		Number = GS ? FMath::Max(0, GS->PlayerArray.IndexOfByKey(PlayerState)) : 0;
	}

	PlayerState->SetPlayerName(FString::Printf(TEXT("Player%d"), Number + 1));
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
