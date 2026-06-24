#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "MTGameInstance.generated.h"

class UMTSessionSubsystem;
class UMTSessionData;

// UI 피드백용 (BP 위젯이 구독). 트래블은 여기(Flow)서만 수행한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMTOnHostResult, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMTOnSearchResult, int32, NumFound);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMTOnJoinResult, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMTOnSessionsFound, const TArray<UMTSessionData*>&, Sessions);

/** 온라인 플로 코디네이터: 세션 이벤트 구독 → 트래블 오케스트레이션. (위젯/서브시스템은 트래블 안 함) */
UCLASS()
class MEOWTRACTIVE_API UMTGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	// UI가 전달하는 "의도"
	UFUNCTION(BlueprintCallable, Category = "MT|Flow")
	void HostGame(int32 NumPublicConnections = 4, bool bIsLAN = false);

	UFUNCTION(BlueprintCallable, Category = "MT|Flow")
	void JoinGame(bool bIsLAN = false);

	// 세션 정리 후 메인메뉴로 복귀
	UFUNCTION(BlueprintCallable, Category = "MT|Flow")
	void LeaveGame();

	// 서버 브라우저에서 선택한 세션 참가
	UFUNCTION(BlueprintCallable, Category = "MT|Flow")
	void JoinSessionData(UMTSessionData* Data);

	// 콘솔(`~`)에서 호출: 현재 로비/메뉴 경로·월드·세션 상태를 로그+화면에 덤프
	UFUNCTION(Exec)
	void MTHostInfo();

	// UI 피드백 이벤트
	UPROPERTY(BlueprintAssignable, Category = "MT|Flow")
	FMTOnHostResult OnHostResult;
	UPROPERTY(BlueprintAssignable, Category = "MT|Flow")
	FMTOnSearchResult OnSearchResult;
	UPROPERTY(BlueprintAssignable, Category = "MT|Flow")
	FMTOnJoinResult OnJoinResult;

	// 검색된 세션 목록 (서버 브라우저가 구독)
	UPROPERTY(BlueprintAssignable, Category = "MT|Flow")
	FMTOnSessionsFound OnSessionsFound;

	// 호스트가 이동할 로비 맵 (BP 자식에서 지정 권장)
	UPROPERTY(EditDefaultsOnly, Category = "MT|Flow")
	FString LobbyPath = TEXT("/Game/Main/Maps/Lobby");

	// 나가기 시 복귀할 메인메뉴 맵
	UPROPERTY(EditDefaultsOnly, Category = "MT|Flow")
	FString MainMenuPath = TEXT("/Game/Main/Maps/MainMenu");

protected:
	// 세션 콜백 → 트래블 수행
	void HandleCreateSession(bool bWasSuccessful);
	void HandleFindSessions(const TArray<FOnlineSessionSearchResult>& Results, bool bWasSuccessful);
	void HandleJoinSession(EOnJoinSessionCompleteResult::Type Result);

private:
	UPROPERTY()
	TObjectPtr<UMTSessionSubsystem> SessionSubsystem;
};
