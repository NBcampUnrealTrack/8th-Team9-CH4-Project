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
}

void UMTGameInstance::Shutdown()
{
	if (SessionSubsystem)
	{
		SessionSubsystem->OnCreateSessionComplete.RemoveAll(this);
		SessionSubsystem->OnFindSessionsComplete.RemoveAll(this);
		SessionSubsystem->OnJoinSessionComplete.RemoveAll(this);
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
	// 결과를 BP용 UMTSessionData로 변환해 서버 브라우저에 전달 (자동 참가 안 함)
	TArray<UMTSessionData*> Sessions;
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
			Sessions.Add(Data);
		}
	}
	OnSearchResult.Broadcast(Sessions.Num());
	OnSessionsFound.Broadcast(Sessions);

	// TODO(임시): 서버 브라우저 붙이기 전 테스트용 — 첫 세션 자동 참가. 브라우저 완성 시 제거.
	if (Sessions.Num() > 0 && SessionSubsystem)
	{
		SessionSubsystem->JoinSession(Sessions[0]->Result);
	}
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
