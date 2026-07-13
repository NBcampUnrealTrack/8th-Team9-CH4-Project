// Fill out your copyright notice in the Description page of Project Settings.


#include "Online/MTSessionSubsystem.h"
#include "Game/MTLog.h"
#include "OnlineSubsystem.h"
#include "Online/OnlineSessionNames.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Engine/LocalPlayer.h"

UMTSessionSubsystem::UMTSessionSubsystem() : CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &UMTSessionSubsystem::HandleCreateSessionComplete)),
										   FindSessionsCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &UMTSessionSubsystem::HandleFindSessionsComplete)),
										   JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &UMTSessionSubsystem::HandleJoinSessionComplete)),
	DestroySessionCompleteDelegate(FOnDestroySessionCompleteDelegate::CreateUObject(this, &UMTSessionSubsystem::HandleDestroySessionComplete))
{
}

void UMTSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 온라인 서브시스템에서 세션 인터페이스 가져오기 (생성자 금지 — CDO 참조 누수)
	if (IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get())
	{
		SessionInterface = Subsystem->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			// 친구 초대 수락 델리게이트 등록 (인게임 수락 시 자동 참가)
			SessionUserInviteAcceptedDelegate = FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &UMTSessionSubsystem::HandleSessionUserInviteAccepted);
			SessionUserInviteAcceptedDelegateHandle = SessionInterface->AddOnSessionUserInviteAcceptedDelegate_Handle(SessionUserInviteAcceptedDelegate);
		}
	}
}

void UMTSessionSubsystem::Deinitialize()
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnSessionUserInviteAcceptedDelegate_Handle(SessionUserInviteAcceptedDelegateHandle);
	}
	// 우리가 들고 있던 세션 인터페이스 공유 참조를 해제해야
	// Null OSS 종료 시 SessionInterface.IsUnique() ensure가 안 터진다.
	SessionInterface.Reset();
	Super::Deinitialize();
}

void UMTSessionSubsystem::ApplyRoomSettingsTo(FOnlineSessionSettings& Settings, const FMTRoomSettings& Room)
{
	Settings.Set(FName("ROOMNAME"), Room.RoomName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(FName("HASPW"), Room.HasPassword() ? 1 : 0, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	// 간이 검증용 평문 광고 — 파티게임 스코프. 강한 보안 필요 시 서버측 PreLogin 검증으로 교체
	Settings.Set(FName("PW"), Room.Password, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(FName("ROOMMODE"), (int32)Room.GameMode, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(FName("ROOMMAP"), (int32)Room.Map, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
}

void UMTSessionSubsystem::UpdateRoomSettings(const FMTRoomSettings& RoomSettings)
{
	if (!SessionInterface.IsValid() || !LastSessionSettings.IsValid())
	{
		return;
	}
	ApplyRoomSettingsTo(*LastSessionSettings, RoomSettings);
	SessionInterface->UpdateSession(NAME_GameSession, *LastSessionSettings, true);
}

void UMTSessionSubsystem::SetSessionJoinable(bool bJoinable)
{
	if (!SessionInterface.IsValid() || !LastSessionSettings.IsValid())
	{
		return;
	}
	LastSessionSettings->bAllowJoinInProgress = bJoinable;
	LastSessionSettings->bShouldAdvertise = bJoinable;   // 검색 목록 노출/숨김도 함께
	SessionInterface->UpdateSession(NAME_GameSession, *LastSessionSettings, true);
}

void UMTSessionSubsystem::CreateSession(int32 NumPublicConnections, bool bIsLAN, const FMTRoomSettings& RoomSettings)
{
	const ULocalPlayer* LocalPlayer = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
	if (!SessionInterface.IsValid() || !LocalPlayer)
	{
		OnCreateSessionComplete.Broadcast(false);
		return;
	}

	// 기존 세션이 있다면 파괴 후 새로 생성
	if(SessionInterface->GetNamedSession(NAME_GameSession))
	{
		SessionInterface->DestroySession(NAME_GameSession);
	}
	
	// 세션 설정
	LastSessionSettings = MakeShareable(new FOnlineSessionSettings());
	LastSessionSettings->bIsLANMatch = bIsLAN;
	LastSessionSettings->NumPublicConnections = NumPublicConnections;
	LastSessionSettings->bShouldAdvertise = true;
	LastSessionSettings->bAllowJoinInProgress = true;
	LastSessionSettings->bAllowInvites = true;          // 친구 초대 허용 (기본 true지만 명시)
	// presence/lobby는 Steam 전용 → LAN(Null)에선 끄지 않으면 조인이 막힌다
	const bool bUseSteamFeatures = !bIsLAN;
	LastSessionSettings->bAllowJoinViaPresence = bUseSteamFeatures;
	LastSessionSettings->bUsesPresence = bUseSteamFeatures;
	LastSessionSettings->bUseLobbiesIfAvailable = bUseSteamFeatures;
	LastSessionSettings->BuildUniqueId = 1;
	LastSessionSettings->Set(FName("MatchType"), MatchType, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	ApplyRoomSettingsTo(*LastSessionSettings, RoomSettings);
	// 세션 생성 완료 델리게이트 등록
	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);

	// 세션 검색 시도 및 세션이 생성되지 않으면 델리게이트 해제 및 실패 브로드캐스트
	UE_CLOG(MTLogEnabled(), LogMT, Log, TEXT("[MTSession] CreateSession 시도 (Conn=%d, LAN=%d)"), NumPublicConnections, bIsLAN ? 1 : 0);
	if(!SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, *LastSessionSettings))
	{
		UE_CLOG(MTLogEnabled(), LogMT, Error, TEXT("[MTSession] CreateSession 즉시 실패 (인터페이스 거부)"));
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		OnCreateSessionComplete.Broadcast(false);
	}
}

void UMTSessionSubsystem::FindSessions(int32 MaxSearchResults, bool bIsLAN)
{
	const ULocalPlayer* LocalPlayer = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr; // 로컬 플레이어 가져오기
	// 예외처리
	if(!SessionInterface.IsValid() || !LocalPlayer)
	{
		OnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
		return;
	}

	// 세션 검색 델리게이트 등록
	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);

	// 세션 검색 설정
	LastSessionSearch = MakeShareable(new FOnlineSessionSearch());
	LastSessionSearch->bIsLanQuery = bIsLAN;
	LastSessionSearch->MaxSearchResults = MaxSearchResults;

	// 로컬로 만나지 않으면 Steam presence로 검색
	if (!bIsLAN)
	{
		// SEARCH_PRESENCE 동일 키를 직접 사용
		LastSessionSearch->QuerySettings.Set(FName(TEXT("PRESENCESEARCH")), true, EOnlineComparisonOp::Equals);
	}

	// 세션 검색 시도 및 검색이 시작되지 않으면 델리게이트 해제 및 실패 브로드캐스트
	if(!SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), LastSessionSearch.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		OnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
	}
}

void UMTSessionSubsystem::JoinSession(const FOnlineSessionSearchResult& SearchResult)
{
	// 컨트롤러 가져오고 예외처리
	const ULocalPlayer* LocalPlayer = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
	if(!SessionInterface.IsValid() || !LocalPlayer)
	{
		OnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		return;
	}

	// 세션 참여 델리게이트 등록
	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);

	// UE5.5 Steam 버그 회피: 찾은 세션의 presence/lobby 플래그가 어긋나면 JoinSession이 거부됨.
	// 조인 직전 사본을 만들어 두 값을 강제로 일치(true)시킨다. (NormalizeFlags)
	// 단 LAN(Null)에선 lobby/presence가 없으므로 건드리지 않는다 (건드리면 조인 깨짐).
	FOnlineSessionSearchResult FixedResult = SearchResult;
	if (!FixedResult.Session.SessionSettings.bIsLANMatch)
	{
		FixedResult.Session.SessionSettings.bUsesPresence = true;
		FixedResult.Session.SessionSettings.bUseLobbiesIfAvailable = true;
	}

	// 세션 참여 시도 및 참여가 시작되지 않으면 델리게이트 해제 및 실패 브로드캐스트
	if (!SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, FixedResult))
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		OnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
	}
}

void UMTSessionSubsystem::DestroySession()
{
	// 세션 예외처리
	if(!SessionInterface.IsValid())
	{
		OnDestroySessionComplete.Broadcast(false);
		return;
	}

	// 세션 파괴 델리게이트 등록
	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);

	// 세션 파괴 시도 및 파괴가 시작되지 않으면 델리게이트 해제 및 실패 브로드캐스트
	if(!SessionInterface->DestroySession(NAME_GameSession))
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		OnDestroySessionComplete.Broadcast(false);
	}
}

bool UMTSessionSubsystem::GetResolvedConnectString(FString& OutConnectString) const
{
	return SessionInterface.IsValid() && SessionInterface->GetResolvedConnectString(NAME_GameSession, OutConnectString);
}

void UMTSessionSubsystem::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	UE_CLOG(MTLogEnabled(), LogMT, Log, TEXT("[MTSession] HandleCreateSessionComplete: Name=%s, Success=%d"),
		*SessionName.ToString(), bWasSuccessful ? 1 : 0);
	// 세션 생성 델리게이트 해제 및 결과 브로드캐스트
	if(SessionInterface.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
	}
	OnCreateSessionComplete.Broadcast(bWasSuccessful);
}

void UMTSessionSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	// 세션 검색 델리게이트 해제
	if(SessionInterface.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
	}

	// 검색 결과 브로드캐스트, MatchType이 일치하는 세션만 필터링하여 브로드캐스트
	TArray<FOnlineSessionSearchResult> Filtered;

	if (LastSessionSearch.IsValid())
	{
		TSet<FString> SeenIds;   // LAN 비콘 재질의로 같은 세션이 중복 응답됨 → ID로 1개만
		for(const FOnlineSessionSearchResult& Result : LastSessionSearch->SearchResults)
		{
			FString FoundMatchType;
			if (Result.Session.SessionSettings.Get(FName("MatchType"), FoundMatchType) && FoundMatchType == MatchType
				&& !SeenIds.Contains(Result.GetSessionIdStr()))
			{
				SeenIds.Add(Result.GetSessionIdStr());
				Filtered.Add(Result);
			}
		}
	}

	UE_CLOG(MTLogEnabled(), LogMT, Log, TEXT("[MTSession] FindSessions 완료: 전체=%d, MatchType일치=%d, Success=%d"),
		LastSessionSearch.IsValid() ? LastSessionSearch->SearchResults.Num() : -1, Filtered.Num(), bWasSuccessful ? 1 : 0);

	OnFindSessionsComplete.Broadcast(Filtered, bWasSuccessful && Filtered.Num() > 0);
}

void UMTSessionSubsystem::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	UE_CLOG(MTLogEnabled(), LogMT, Log, TEXT("[MTSession] JoinSession 완료: Result=%d (0=Success)"), (int32)Result);
	// 세션 참여 델리게이트 해제 및 결과 브로드캐스트
	if(SessionInterface.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
	}
	OnJoinSessionComplete.Broadcast(Result);
}

void UMTSessionSubsystem::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	// 세션 파괴 델리게이트 해제 및 결과 브로드캐스트
	if(SessionInterface.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
	}
	OnDestroySessionComplete.Broadcast(bWasSuccessful);
}

void UMTSessionSubsystem::HandleSessionUserInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
	// 친구 초대 수락 → 해당 세션 참가 (성공 시 기존 Join 흐름 → 트래블)
	if (bWasSuccessful && InviteResult.IsValid())
	{
		JoinSession(InviteResult);
	}
}

void UMTSessionSubsystem::ShowInviteUI()
{
	// 세션에 있을 때 Steam 오버레이의 친구 초대 창을 연다 (Steam 전용)
	if (IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get())
	{
		if (IOnlineExternalUIPtr ExternalUI = Subsystem->GetExternalUIInterface())
		{
			ExternalUI->ShowInviteUI(0, NAME_GameSession);
		}
	}
}
