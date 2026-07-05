#include "Game/MTGameInstance.h"
#include "Game/MTLog.h"
#include "Online/MTSessionSubsystem.h"
#include "Online/MTSessionData.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
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
	Super::Shutdown();
}

void UMTGameInstance::HostGame(int32 NumPublicConnections, bool bIsLAN)
{
	MTScreen(FColor::Cyan, FString::Printf(TEXT("[MTHost] HostGame (Conn=%d, LAN=%d, SS=%d)"),
		NumPublicConnections, bIsLAN ? 1 : 0, SessionSubsystem ? 1 : 0));
	if (SessionSubsystem)
	{
		SessionSubsystem->CreateSession(NumPublicConnections, bIsLAN);
	}
	else
	{
		MTScreen(FColor::Red, TEXT("[MTHost] SessionSubsystem null → 방생성 불가"));
	}
}

void UMTGameInstance::JoinGame(bool bIsLAN)
{
	if (SessionSubsystem)
	{
		SessionSubsystem->FindSessions(10000, bIsLAN);
	}
}

void UMTGameInstance::LeaveGame()
{
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

void UMTGameInstance::JoinSessionData(UMTSessionData* Data)
{
	if (SessionSubsystem && Data)
	{
		SessionSubsystem->JoinSession(Data->Result);
	}
}

void UMTGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
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
	PendingDisconnectMessage = FText::FromString(Reason + TEXT("\n메인 메뉴로 돌아갑니다."));
	bHasPendingDisconnect = true;

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
		MTScreen(FColor::Red, TEXT("[MTHost] 세션 생성 실패 → 트래블 안 함"));
		return;
	}
	if (!GetWorld())
	{
		MTScreen(FColor::Red, TEXT("[MTHost] World null → 트래블 불가"));
		return;
	}

	const FString TravelURL = LobbyPath + TEXT("?listen");
	const bool bTraveling = GetWorld()->ServerTravel(TravelURL);
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
			Data->ServerName = Result.Session.OwningUserName;
			Data->PingMs = Result.PingInMs;
			Data->MaxSlots = Result.Session.SessionSettings.NumPublicConnections;
			Data->OpenSlots = Result.Session.NumOpenPublicConnections;
			Data->CurrentPlayers = FMath::Max(0, Data->MaxSlots - Data->OpenSlots);
			FoundSessions.Add(Data);   // GC 방지 캐시
			Sessions.Add(Data);

			MTScreen(FColor::Cyan, FString::Printf(TEXT("[MTFind]  #%d %s | 인원 %d/%d | %dms"),
				Sessions.Num() - 1, *Data->ServerName, Data->CurrentPlayers, Data->MaxSlots, Data->PingMs));
		}
	}
	MTScreen(Sessions.Num() > 0 ? FColor::Green : FColor::Orange,
		FString::Printf(TEXT("[MTFind] 검색 완료: Success=%d, 세션 %d개 → OnSessionsFound 방송"),
			bWasSuccessful ? 1 : 0, Sessions.Num()));

	OnSearchResult.Broadcast(Sessions.Num());
	OnSessionsFound.Broadcast(Sessions);
}

void UMTGameInstance::HandleJoinSession(EOnJoinSessionCompleteResult::Type Result)
{
	FString Address;
	const bool bOk = (Result == EOnJoinSessionCompleteResult::Success)
		&& SessionSubsystem && SessionSubsystem->GetResolvedConnectString(Address);

	OnJoinResult.Broadcast(bOk);
	if (bOk)
	{
		if (APlayerController* PC = GetFirstLocalPlayerController())
		{
			PC->ClientTravel(Address, TRAVEL_Absolute);
		}
	}
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
