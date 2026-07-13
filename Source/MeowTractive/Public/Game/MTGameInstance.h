#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Game/MTTypes.h"
#include "MTGameInstance.generated.h"

class UMTSessionSubsystem;
class UMTSessionData;
class USoundMix;
class USoundClass;

// UI 피드백용 (BP 위젯이 구독). 트래블은 여기(Flow)서만 수행한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMTOnHostResult, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMTOnSearchResult, int32, NumFound);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMTOnJoinResult, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMTOnJoinFailed, FText, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMTOnSessionsFound, const TArray<UMTSessionData*>&, Sessions);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMTOnConnectingStateChanged, bool, bConnecting);

/** 온라인 플로 코디네이터: 세션 이벤트 구독 → 트래블 오케스트레이션. (위젯/서브시스템은 트래블 안 함) */
UCLASS()
class MEOWTRACTIVE_API UMTGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	// UI가 전달하는 "의도" — 방이름 비면 호스트 닉네임, 비번 비면 공개방
	UFUNCTION(BlueprintCallable, Category = "MT|Flow")
	void HostGame(FMTRoomSettings RoomSettings, int32 NumPublicConnections = 4, bool bIsLAN = false);

	UFUNCTION(BlueprintCallable, Category = "MT|Flow")
	void JoinGame(bool bIsLAN = false);

	// 빠른 시작: 검색 → 참가 가능한 공개방 있으면 첫 방 참가, 없으면 기본 설정으로 방 생성
	UFUNCTION(BlueprintCallable, Category = "MT|Flow")
	void QuickStart(int32 NumPublicConnections = 4, bool bIsLAN = false);

	// 세션 정리 후 메인메뉴로 복귀
	UFUNCTION(BlueprintCallable, Category = "MT|Flow")
	void LeaveGame();

	// 서버 브라우저에서 선택한 세션 참가 (비밀번호 방이면 Password 필요. 초대 경로는 검증 없음)
	UFUNCTION(BlueprintCallable, Category = "MT|Flow")
	void JoinSessionData(UMTSessionData* Data, const FString& Password = TEXT(""));

	// 방 이름으로 참가 (마지막 검색 결과에서 대소문자 무시 검색)
	UFUNCTION(BlueprintCallable, Category = "MT|Flow")
	void JoinSessionByName(const FString& RoomName, const FString& Password);

	// 현재 방 설정 (호스트 로컬 기준. 로비 표시는 MTLobbyGameState 복제값 사용)
	UFUNCTION(BlueprintPure, Category = "MT|Flow")
	FMTRoomSettings GetRoomSettings() const { return CurrentRoomSettings; }

	// 로비에서 방 설정 변경 시 세션 광고 갱신 (LobbyGameMode가 호출)
	void ApplyRoomSettings(const FMTRoomSettings& NewSettings);

	// 강퇴 등 외부 사유로 메뉴 복귀 시 표시할 메시지 지정 (이미 있으면 유지)
	void SetPendingDisconnectMessage(const FText& Message);

	// 세션 튕김/종료로 메뉴 복귀 시 대기 중인 안내 메시지를 1회 소비 — 메뉴 위젯이 Construct에서 호출
	UFUNCTION(BlueprintCallable, Category = "MT|Flow")
	bool ConsumeDisconnectMessage(FText& OutMessage);

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

	// 참가 실패 시 사유 텍스트 (메뉴가 구독해 MessageDialog로 표시)
	UPROPERTY(BlueprintAssignable, Category = "MT|Flow")
	FMTOnJoinFailed OnJoinFailed;

	// 검색된 세션 목록 (서버 브라우저가 구독)
	UPROPERTY(BlueprintAssignable, Category = "MT|Flow")
	FMTOnSessionsFound OnSessionsFound;

	// 접속 플로(호스트/참가) 진행 상태 — 메뉴가 구독해 "접속 중" 창 표시 (중복 클릭 차단은 Flow가 수행)
	UPROPERTY(BlueprintAssignable, Category = "MT|Flow")
	FMTOnConnectingStateChanged OnConnectingStateChanged;

	// 접속 진행 중 여부 (메뉴 재진입 시 초기 표시용)
	UFUNCTION(BlueprintPure, Category = "MT|Flow")
	bool IsConnecting() const { return bConnecting; }

	// 호스트가 이동할 로비 맵 (쿠커가 소프트 참조로 추적하게 BP 자식에서 지정)
	UPROPERTY(EditDefaultsOnly, Category = "MT|Flow")
	TSoftObjectPtr<UWorld> LobbyMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Main/Maps/Lobby.Lobby")));

	// 나가기 시 복귀할 메인메뉴 맵
	UPROPERTY(EditDefaultsOnly, Category = "MT|Flow")
	TSoftObjectPtr<UWorld> MainMenuMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Main/Maps/MainMenu.MainMenu")));

	// 트래블용 패키지 경로 (매치→로비 복귀 시 AMTMatchGameMode도 사용)
	FString GetLobbyMapPath() const { return LobbyMap.ToSoftObjectPath().GetLongPackageName(); }
	FString GetMainMenuMapPath() const { return MainMenuMap.ToSoftObjectPath().GetLongPackageName(); }

	// 저장된 볼륨(UMTGameUserSettings)을 사운드 믹스에 반영 (맵 로드/설정 변경 시 호출)
	UFUNCTION(BlueprintCallable, Category = "MT|Audio")
	void ApplyAudioSettings();

	// 볼륨 조절용 믹스/클래스 (BP_MTGameInstance에서 지정)
	UPROPERTY(EditDefaultsOnly, Category = "MT|Audio")
	TObjectPtr<USoundMix> VolumeMix;

	UPROPERTY(EditDefaultsOnly, Category = "MT|Audio")
	TObjectPtr<USoundClass> MasterSoundClass;

	UPROPERTY(EditDefaultsOnly, Category = "MT|Audio")
	TObjectPtr<USoundClass> BGMSoundClass;

	UPROPERTY(EditDefaultsOnly, Category = "MT|Audio")
	TObjectPtr<USoundClass> SFXSoundClass;

protected:
	// 세션 콜백 → 트래블 수행
	void HandleCreateSession(bool bWasSuccessful);
	void HandleFindSessions(const TArray<FOnlineSessionSearchResult>& Results, bool bWasSuccessful);
	void HandleJoinSession(EOnJoinSessionCompleteResult::Type Result);

	// 세션 연결 끊김(튕김/호스트 종료 등) — GEngine->OnNetworkFailure 구독. 메시지 저장 후 메인메뉴 복귀.
	void HandleNetworkFailure(UWorld* World, class UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);

	// 맵 로드 완료 = 접속 플로 종료 (성공/복귀 공통 캐치올)
	void HandlePostLoadMap(UWorld* LoadedWorld);

private:
	// 가드 없는 실제 호스트 수행 (QuickStart 내부 경로 공용)
	void HostGameInternal(FMTRoomSettings RoomSettings, int32 NumPublicConnections, bool bIsLAN);

	// 접속 진행 상태 전환 + 변경 시 방송
	void SetConnecting(bool bNewConnecting);

	UPROPERTY()
	TObjectPtr<UMTSessionSubsystem> SessionSubsystem;

	// true 동안 호스트/참가/빠른시작 재요청 무시 (중복 클릭 버그 방지)
	bool bConnecting = false;

	// 검색 결과 캐시 — ListView가 참조를 잡기 전 GC되지 않도록 보관 (다음 검색 때 갱신)
	UPROPERTY()
	TArray<TObjectPtr<UMTSessionData>> FoundSessions;

	// 메뉴 복귀 시 표시할 튕김 안내 (1회 소비)
	FText PendingDisconnectMessage;
	bool bHasPendingDisconnect = false;

	// 호스트가 만든/변경한 방 설정 (로비 GameState 초기화·세션 광고 갱신용)
	FMTRoomSettings CurrentRoomSettings;

	// 빠른 시작 대기 상태 (검색 완료 콜백에서 1회 소비)
	bool bQuickStartPending = false;
	int32 QuickStartConnections = 4;
	bool bQuickStartLAN = false;

	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle PostLoadMapHandle;
};
