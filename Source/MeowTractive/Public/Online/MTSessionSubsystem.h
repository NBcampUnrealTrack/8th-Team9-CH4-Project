// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Game/MTTypes.h"
#include "MTSessionSubsystem.generated.h"

// 세션 생성, 검색, 참가 완료 시 델리게이트 선언
DECLARE_MULTICAST_DELEGATE_OneParam(FMTOnCreateSessionComplete, bool /*bWasSuccessful*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMTOnFindSessionsComplete, const TArray<FOnlineSessionSearchResult>& /*SessionResults*/, bool /*bWasSuccessful*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FMTOnJoinSessionComplete, EOnJoinSessionCompleteResult::Type /*Result*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FMTOnDestroySessionComplete, bool /*bWasSuccessful*/);

/**
 * 
 */
UCLASS()
class MEOWTRACTIVE_API UMTSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UMTSessionSubsystem();

	// 종료 시 세션 인터페이스 참조 해제 (Null OSS의 IsUnique ensure 방지)
	virtual void Deinitialize() override;

	// 방 생성 (방이름/비번/모드/맵은 세션 설정으로 광고)
	UFUNCTION(BlueprintCallable, Category = "MT|Session")
	void CreateSession(int32 NumPublicConnections, bool bIsLAN, const FMTRoomSettings& RoomSettings);

	// 로비 중 방 설정 변경 → 세션 광고 갱신 (호스트 전용)
	void UpdateRoomSettings(const FMTRoomSettings& RoomSettings);

	// 방 검색
	UFUNCTION(BlueprintCallable, Category = "MT|Session")
	void FindSessions(int32 MaxSearchResults, bool bIsLAN);

	// 방 참여
	void JoinSession(const FOnlineSessionSearchResult& SearchResult);

	// 방 파괴
	UFUNCTION(BlueprintCallable, Category = "MT|Session")
	void DestroySession();

	// 방 참여 후 클라이언트용 접속 주소
	bool GetResolvedConnectString(FString& OutConnectString) const;

	// Steam 오버레이 친구 초대 UI 열기 (세션에 들어가 있을 때만 의미 있음)
	UFUNCTION(BlueprintCallable, Category = "MT|Session")
	void ShowInviteUI();

	// UI 결과 델리게이트
	FMTOnCreateSessionComplete OnCreateSessionComplete;
	FMTOnFindSessionsComplete OnFindSessionsComplete;
	FMTOnJoinSessionComplete OnJoinSessionComplete;
	FMTOnDestroySessionComplete OnDestroySessionComplete;

protected:
	// 세션 생성시 콜백
	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	// 세션 검색 완료 시 콜백
	void HandleFindSessionsComplete(bool bWasSuccessful);
	// 세션 참여 완료 시 콜백
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	// 세션 파괴 완료 시 콜백
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);

	// 친구 초대 수락 시 → 해당 세션 참가
	void HandleSessionUserInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);

private:
	// 방 설정을 세션 광고 키로 기록 (Create/Update 공용)
	static void ApplyRoomSettingsTo(FOnlineSessionSettings& Settings, const FMTRoomSettings& Room);

	IOnlineSessionPtr SessionInterface; // 온라인 세션 인터페이스
	TSharedPtr<FOnlineSessionSearch> LastSessionSearch; // 마지막 세션 검색 결과 저장
	TSharedPtr<FOnlineSessionSettings> LastSessionSettings; // 마지막 세션 설정 저장

	// 세션 구분 키
	FString MatchType = TEXT("MeowTractive");

	// 델리게이트 + 핸들 (콜백에서 해제)
	FOnCreateSessionCompleteDelegate CreateSessionCompleteDelegate;
	FDelegateHandle CreateSessionCompleteDelegateHandle;

	FOnFindSessionsCompleteDelegate FindSessionsCompleteDelegate;
	FDelegateHandle FindSessionsCompleteDelegateHandle;

	FOnJoinSessionCompleteDelegate JoinSessionCompleteDelegate;
	FDelegateHandle JoinSessionCompleteDelegateHandle;

	FOnDestroySessionCompleteDelegate DestroySessionCompleteDelegate;
	FDelegateHandle DestroySessionCompleteDelegateHandle;

	// 친구 초대 수락 델리게이트
	FOnSessionUserInviteAcceptedDelegate SessionUserInviteAcceptedDelegate;
	FDelegateHandle SessionUserInviteAcceptedDelegateHandle;
};
