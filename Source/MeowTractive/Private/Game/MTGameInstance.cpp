#include "Game/MTGameInstance.h"
#include "Game/MTLog.h"
#include "Game/MTGameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Online/MTSessionSubsystem.h"
#include "Online/MTSessionData.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Misc/PackageName.h"

// 로그 + 화면 동시 출력. bForce=true면 MT.Log 토글 무시 (수동 명령용)
static void MTScreen(const FColor& Color, const FString& Msg, bool bForce = false)
{
	if (!bForce && !MTLogEnabled())
	{
		return;
	}
	UE_LOG(LogMT, Log, TEXT("%s"), *Msg);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 6.f, Color, Msg);
	}
}

void UMTGameInstance::Init()
{
	Super::Init();

	SessionSubsystem = GetSubsystem<UMTSessionSubsystem>();
	if (SessionSubsystem)
	{
		SessionSubsystem->OnCreateSessionComplete.AddUObject(this, &UMTGameInstance::HandleCreateSession);
		SessionSubsystem->OnFindSessionsComplete.AddUObject(this, &UMTGameInstance::HandleFindSessions);
		SessionSubsystem->OnJoinSessionComplete.AddUObject(this, &UMTGameInstance::HandleJoinSession);
	}

	// 세션 튕김/연결 끊김 감지
	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UMTGameInstance::HandleNetworkFailure);
	}

	// 트래블 완료(로비 진입/메뉴 복귀) 시 접속 상태 해제
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UMTGameInstance::HandlePostLoadMap);
}

void UMTGameInstance::Shutdown()
{
	if (SessionSubsystem)
	{
		SessionSubsystem->OnCreateSessionComplete.RemoveAll(this);
		SessionSubsystem->OnFindSessionsComplete.RemoveAll(this);
		SessionSubsystem->OnJoinSessionComplete.RemoveAll(this);
	}
	if (GEngine && NetworkFailureHandle.IsValid())
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		NetworkFailureHandle.Reset();
	}
	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}
	Super::Shutdown();
}

void UMTGameInstance::HostGame(FMTRoomSettings RoomSettings, int32 NumPublicConnections, bool bIsLAN)
{
	if (bConnecting)
	{
		MTScreen(FColor::Yellow, TEXT("[MTHost] 이미 접속 진행 중 → 호스트 요청 무시"));
		return;
	}
	HostGameInternal(RoomSettings, NumPublicConnections, bIsLAN);
}

void UMTGameInstance::HostGameInternal(FMTRoomSettings RoomSettings, int32 NumPublicConnections, bool bIsLAN)
{
	// 방이름 미입력 → 호스트 닉네임
	if (RoomSettings.RoomName.TrimStartAndEnd().IsEmpty())
	{
		const ULocalPlayer* LP = GetFirstGamePlayer();
		RoomSettings.RoomName = LP ? LP->GetNickname() : TEXT("MeowTractive");
	}
	CurrentRoomSettings = RoomSettings;

	MTScreen(FColor::Cyan, FString::Printf(TEXT("[MTHost] HostGame '%s' (PW=%d, Mode=%d, Map=%d, Conn=%d, LAN=%d)"),
		*RoomSettings.RoomName, RoomSettings.HasPassword() ? 1 : 0,
		(int32)RoomSettings.GameMode, (int32)RoomSettings.Map, NumPublicConnections, bIsLAN ? 1 : 0));

	if (SessionSubsystem)
	{
		SetConnecting(true);
		SessionSubsystem->CreateSession(NumPublicConnections, bIsLAN, RoomSettings);
	}
	else
	{
		MTScreen(FColor::Red, TEXT("[MTHost] SessionSubsystem null → 방생성 불가"));
	}
}

void UMTGameInstance::SetConnecting(bool bNewConnecting)
{
	if (bConnecting == bNewConnecting)
	{
		return;
	}
	bConnecting = bNewConnecting;
	OnConnectingStateChanged.Broadcast(bConnecting);
}

void UMTGameInstance::HandlePostLoadMap(UWorld* LoadedWorld)
{
	SetConnecting(false);
	ApplyAudioSettings();   // 맵 전환마다 저장 볼륨 재적용 (믹스 모디파이어는 월드 단위)
}

void UMTGameInstance::ApplyAudioSettings()
{
	UWorld* World = GetWorld();
	const UMTGameUserSettings* Settings = UMTGameUserSettings::GetMTSettings();
	if (!World || !Settings || !VolumeMix)
	{
		return;
	}

	auto ApplyClassVolume = [&](USoundClass* SoundClass, float Volume)
	{
		if (SoundClass)
		{
			UGameplayStatics::SetSoundMixClassOverride(World, VolumeMix, SoundClass, Volume, 1.f, 0.f, true);
		}
	};
	ApplyClassVolume(MasterSoundClass, Settings->GetMasterVolume());
	ApplyClassVolume(BGMSoundClass, Settings->GetBGMVolume());
	ApplyClassVolume(SFXSoundClass, Settings->GetSFXVolume());

	UGameplayStatics::PushSoundMixModifier(World, VolumeMix);
}

void UMTGameInstance::ApplyRoomSettings(const FMTRoomSettings& NewSettings)
{
	CurrentRoomSettings = NewSettings;
	if (SessionSubsystem)
	{
		SessionSubsystem->UpdateRoomSettings(NewSettings);
	}
}

void UMTGameInstance::SetPendingDisconnectMessage(const FText& Message)
{
	// 먼저 온 구체적 사유(강퇴 등)를 나중의 일반 사유가 덮지 않게
	if (!bHasPendingDisconnect)
	{
		PendingDisconnectMessage = Message;
		bHasPendingDisconnect = true;
	}
}

void UMTGameInstance::JoinGame(bool bIsLAN)
{
	if (bConnecting)
	{
		return;
	}
	if (SessionSubsystem)
	{
		SessionSubsystem->FindSessions(50, bIsLAN);
	}
}

void UMTGameInstance::QuickStart(int32 NumPublicConnections, bool bIsLAN)
{
	if (!SessionSubsystem)
	{
		return;
	}
	if (bConnecting)
	{
		MTScreen(FColor::Yellow, TEXT("[MTQuick] 이미 접속 진행 중 → 빠른 시작 무시"));
		return;
	}
	SetConnecting(true);
	// 검색 완료 콜백(HandleFindSessions)에서 참가/생성 분기
	bQuickStartPending = true;
	QuickStartConnections = NumPublicConnections;
	bQuickStartLAN = bIsLAN;

	MTScreen(FColor::Cyan, TEXT("[MTQuick] 빠른 시작: 세션 검색 중..."));
	SessionSubsystem->FindSessions(50, bIsLAN);
}

void UMTGameInstance::LeaveGame()
{
	SetConnecting(false);
	if (SessionSubsystem)
	{
		SessionSubsystem->DestroySession();   // 세션 정리 (비동기, fire-and-forget)
	}
	// 서버/클라 공통: 메인메뉴로 복귀(연결 해제)
	if (APlayerController* PC = GetFirstLocalPlayerController())
	{
		PC->ClientTravel(MainMenuPath, TRAVEL_Absolute);
	}
}

void UMTGameInstance::JoinSessionData(UMTSessionData* Data, const FString& Password)
{
	if (!SessionSubsystem || !Data)
	{
		return;
	}
	if (bConnecting)
	{
		MTScreen(FColor::Yellow, TEXT("[MTFlow] 이미 접속 진행 중 → 참가 요청 무시"));
		return;
	}

	// 간이 비밀번호 검증 (초대 수락 경로는 서브시스템 직행이라 검증 없음)
	if (Data->bHasPassword && !Password.Equals(Data->AdvertisedPassword))
	{
		OnJoinFailed.Broadcast(FText::FromString(TEXT("비밀번호가 틀렸습니다.")));
		return;
	}

	SetConnecting(true);
	SessionSubsystem->JoinSession(Data->Result);
}

void UMTGameInstance::JoinSessionByName(const FString& RoomName, const FString& Password)
{
	if (bConnecting)
	{
		return;
	}
	const FString Trimmed = RoomName.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		OnJoinFailed.Broadcast(FText::FromString(TEXT("방 이름을 입력해 주세요.")));
		return;
	}

	for (UMTSessionData* Data : FoundSessions)
	{
		if (Data && Data->ServerName.Equals(Trimmed, ESearchCase::IgnoreCase))
		{
			JoinSessionData(Data, Password);
			return;
		}
	}
	OnJoinFailed.Broadcast(FText::FromString(TEXT("해당 이름의 방을 찾을 수 없습니다.\n새로고침 후 다시 시도해 주세요.")));
}

void UMTGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	// 조인 시도가 접속 전에 거부됨(예: 매치 진행 중 PreLogin 거부) — 메뉴에 그대로 있으니
	// 서버가 보낸 사유(ErrorString)를 기존 참가 실패 다이얼로그로 표시.
	if (FailureType == ENetworkFailure::PendingConnectionFailure)
	{
		SetConnecting(false);
		const FString Reason = ErrorString.IsEmpty()
			? TEXT("세션에 참여할 수 없습니다.")
			: ErrorString;
		MTScreen(FColor::Orange, FString::Printf(TEXT("[MTFlow] 참가 거부: %s"), *Reason));
		OnJoinFailed.Broadcast(FText::FromString(Reason));
		return;
	}

	// 호스트(리슨 서버)는 자기 세션 유지 — 튕김 복귀는 클라 전용
	if (World && World->GetNetMode() != NM_Client)
	{
		return;
	}

	// 실패 사유별 안내 (표시는 메뉴 위젯이 ConsumeDisconnectMessage로 가져감)
	FString Reason;
	switch (FailureType)
	{
	case ENetworkFailure::ConnectionLost:
	case ENetworkFailure::ConnectionTimeout:
		Reason = TEXT("서버와의 연결이 끊어졌습니다.");
		break;
	case ENetworkFailure::FailureReceived:
		Reason = TEXT("서버에서 연결이 종료되었습니다.");
		break;
	default:
		Reason = TEXT("세션 연결이 종료되었습니다.");
		break;
	}
	// 강퇴 등 먼저 기록된 구체 사유가 있으면 유지됨
	SetPendingDisconnectMessage(FText::FromString(Reason + TEXT("\n메인 메뉴로 돌아갑니다.")));

	MTScreen(FColor::Red, FString::Printf(TEXT("[MTFlow] NetworkError(%d) → 메인메뉴 복귀"), (int32)FailureType));

	// 메인메뉴로 복귀
	if (APlayerController* PC = GetFirstLocalPlayerController())
	{
		PC->ClientTravel(MainMenuPath, TRAVEL_Absolute);
	}
}

bool UMTGameInstance::ConsumeDisconnectMessage(FText& OutMessage)
{
	if (!bHasPendingDisconnect)
	{
		return false;
	}
	OutMessage = PendingDisconnectMessage;
	bHasPendingDisconnect = false;   // 1회성 — 이후 메뉴 재진입엔 안 뜸
	return true;
}

void UMTGameInstance::HandleCreateSession(bool bWasSuccessful)
{
	OnHostResult.Broadcast(bWasSuccessful);

	MTScreen(FColor::Cyan, FString::Printf(TEXT("[MTHost] CreateSession 완료: Success=%d, World=%d, LobbyPath=%s"),
		bWasSuccessful ? 1 : 0, GetWorld() ? 1 : 0, *LobbyPath));

	if (!bWasSuccessful)
	{
		SetConnecting(false);
		MTScreen(FColor::Red, TEXT("[MTHost] 세션 생성 실패 → 트래블 안 함"));
		return;
	}
	if (!GetWorld())
	{
		SetConnecting(false);
		MTScreen(FColor::Red, TEXT("[MTHost] World null → 트래블 불가"));
		return;
	}

	const FString TravelURL = LobbyPath + TEXT("?listen");
	const bool bTraveling = GetWorld()->ServerTravel(TravelURL);
	if (!bTraveling)
	{
		SetConnecting(false);
	}
	MTScreen(bTraveling ? FColor::Green : FColor::Red,
		FString::Printf(TEXT("[MTHost] ServerTravel(\"%s\") → %s"),
			*TravelURL, bTraveling ? TEXT("시작됨") : TEXT("거부됨 (경로/맵 존재 확인!)")));
}

void UMTGameInstance::HandleFindSessions(const TArray<FOnlineSessionSearchResult>& Results, bool bWasSuccessful)
{
	// 결과를 BP용 UMTSessionData로 변환해 서버 브라우저에 전달 (선택 후 수동 참가)
	FoundSessions.Reset();
	TArray<UMTSessionData*> Sessions;   // 방송용 (dynamic 델리게이트는 raw 포인터 배열)
	if (bWasSuccessful)
	{
		for (const FOnlineSessionSearchResult& Result : Results)
		{
			UMTSessionData* Data = NewObject<UMTSessionData>(this);
			Data->Result = Result;
			Data->MaxSlots = Result.Session.SessionSettings.NumPublicConnections;
			Data->OpenSlots = Result.Session.NumOpenPublicConnections;
			Data->CurrentPlayers = FMath::Max(0, Data->MaxSlots - Data->OpenSlots);

			// 방 설정 광고값 읽기 (없으면 기본값 유지)
			FString RoomName;
			Result.Session.SessionSettings.Get(FName("ROOMNAME"), RoomName);
			Data->ServerName = RoomName.IsEmpty() ? Result.Session.OwningUserName : RoomName;

			int32 HasPw = 0, ModeInt = 0, MapInt = 0;
			Result.Session.SessionSettings.Get(FName("HASPW"), HasPw);
			Result.Session.SessionSettings.Get(FName("PW"), Data->AdvertisedPassword);
			Result.Session.SessionSettings.Get(FName("ROOMMODE"), ModeInt);
			Result.Session.SessionSettings.Get(FName("ROOMMAP"), MapInt);
			Data->bHasPassword = HasPw != 0;
			Data->GameMode = (EMTRoomGameMode)ModeInt;
			Data->Map = (EMTRoomMap)MapInt;

			FoundSessions.Add(Data);   // GC 방지 캐시
			Sessions.Add(Data);
		}

		// 열린 방(비번 없음) 먼저 — 핑은 Steam 로비 검색에서 미제공이라 정렬 기준에서 제외
		Sessions.Sort([](const UMTSessionData& A, const UMTSessionData& B)
		{
			return !A.bHasPassword && B.bHasPassword;
		});

		for (int32 i = 0; i < Sessions.Num(); ++i)
		{
			MTScreen(FColor::Cyan, FString::Printf(TEXT("[MTFind]  #%d %s | 인원 %d/%d | PW=%d"),
				i, *Sessions[i]->ServerName, Sessions[i]->CurrentPlayers, Sessions[i]->MaxSlots,
				Sessions[i]->bHasPassword ? 1 : 0));
		}
	}
	MTScreen(Sessions.Num() > 0 ? FColor::Green : FColor::Orange,
		FString::Printf(TEXT("[MTFind] 검색 완료: Success=%d, 세션 %d개 → OnSessionsFound 방송"),
			bWasSuccessful ? 1 : 0, Sessions.Num()));

	// 빠른 시작: 참가 가능한 공개방(자리 있음) 첫 방 참가, 없으면 기본 설정으로 방 생성
	if (bQuickStartPending)
	{
		bQuickStartPending = false;

		for (UMTSessionData* Data : Sessions)
		{
			if (Data && !Data->bHasPassword && Data->OpenSlots > 0)
			{
				MTScreen(FColor::Green, FString::Printf(TEXT("[MTQuick] 참가 가능 방 발견 → '%s' 참가"), *Data->ServerName));
				SessionSubsystem->JoinSession(Data->Result);
				return;
			}
		}

		MTScreen(FColor::Yellow, TEXT("[MTQuick] 참가 가능한 방 없음 → 기본 설정으로 방 생성"));
		HostGameInternal(FMTRoomSettings(), QuickStartConnections, bQuickStartLAN);   // 접속 진행 중이므로 가드 우회
		return;
	}

	OnSearchResult.Broadcast(Sessions.Num());
	OnSessionsFound.Broadcast(Sessions);
}

void UMTGameInstance::HandleJoinSession(EOnJoinSessionCompleteResult::Type Result)
{
	FString Address;
	const bool bOk = (Result == EOnJoinSessionCompleteResult::Success)
		&& SessionSubsystem && SessionSubsystem->GetResolvedConnectString(Address);

	if (!bOk)
	{
		SetConnecting(false);
	}
	OnJoinResult.Broadcast(bOk);
	if (bOk)
	{
		if (APlayerController* PC = GetFirstLocalPlayerController())
		{
			PC->ClientTravel(Address, TRAVEL_Absolute);
		}
		return;
	}

	// 실패 사유 → 안내 문구 (메뉴가 OnJoinFailed 구독 → MessageDialog 표시)
	FString Reason;
	switch (Result)
	{
	case EOnJoinSessionCompleteResult::SessionIsFull:
		Reason = TEXT("방이 가득 찼습니다.");
		break;
	case EOnJoinSessionCompleteResult::SessionDoesNotExist:
		Reason = TEXT("세션이 더 이상 존재하지 않습니다.\n목록을 새로고침해 주세요.");
		break;
	case EOnJoinSessionCompleteResult::AlreadyInSession:
		Reason = TEXT("이미 세션에 참가해 있습니다.");
		break;
	case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress:
	default:
		Reason = TEXT("세션 참가에 실패했습니다.");
		break;
	}

	MTScreen(FColor::Orange, FString::Printf(TEXT("[MTFlow] Join 실패 (Result=%d): %s"), (int32)Result, *Reason));
	OnJoinFailed.Broadcast(FText::FromString(Reason));
}

void UMTGameInstance::MTHostInfo()
{
	// 경로의 맵 패키지가 실제로 존재하는지까지 검사 → "로비로 안 감" 1차 진단
	const bool bLobbyExists = FPackageName::DoesPackageExist(LobbyPath);
	const bool bMenuExists = FPackageName::DoesPackageExist(MainMenuPath);

	MTScreen(FColor::Yellow, FString::Printf(TEXT("[MTHostInfo] LobbyPath=%s (맵 존재=%d)"), *LobbyPath, bLobbyExists ? 1 : 0), true);
	MTScreen(FColor::Yellow, FString::Printf(TEXT("[MTHostInfo] MainMenuPath=%s (맵 존재=%d)"), *MainMenuPath, bMenuExists ? 1 : 0), true);
	MTScreen(FColor::Yellow, FString::Printf(TEXT("[MTHostInfo] World=%s, SessionSubsystem=%d"),
		GetWorld() ? *GetWorld()->GetMapName() : TEXT("null"), SessionSubsystem ? 1 : 0), true);

	if (!bLobbyExists)
	{
		MTScreen(FColor::Red, TEXT("[MTHostInfo] 로비 맵을 못 찾음! LobbyPath 경로가 틀렸습니다."), true);
	}
}
