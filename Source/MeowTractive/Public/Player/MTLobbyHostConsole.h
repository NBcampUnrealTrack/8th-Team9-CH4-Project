#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/MTLobbySelectable.h"
#include "MTLobbyHostConsole.generated.h"

class UBoxComponent;
class UWidgetComponent;
class UUserWidget;

/**
 * 로비 호스트 전용 콘솔 조형물: 호스트에게만 보이고, 펀치(C) 시 지정 위젯을 연다.
 * BP 서브클래스 2종(방설정/강퇴)이 메시·프롬프트·ConsoleWidgetClass를 지정해 로비 맵에 배치.
 * 고양이 선택 조형물(AMTLobbyCharacter)과 같은 IMTLobbySelectable 경로로 동작한다.
 */
UCLASS(Abstract, Blueprintable)
class MEOWTRACTIVE_API AMTLobbyHostConsole : public AActor, public IMTLobbySelectable
{
	GENERATED_BODY()

public:
	AMTLobbyHostConsole();

	// 서버: 펀치 히트 → 호스트 검증 → 로컬(리슨 호스트) 위젯 열기
	virtual void OnPunchSelect(AActor* InstigatorPawn) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 펀치 판정용 — GA_MeowPunch가 Pawn 오브젝트 타입만 조회하므로 Pawn으로 설정됨
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MT|Lobby")
	TObjectPtr<UBoxComponent> InteractionBox;

	// "[키] 행동" 프롬프트 (WidgetClass는 BP에서 지정 — WBP_Select)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MT|Lobby")
	TObjectPtr<UWidgetComponent> PromptWidget;

	// 프롬프트에 표시할 행동 이름 (BP 서브클래스가 지정: "방 설정" / "플레이어 목록")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MT|Lobby")
	FText PromptLabel = NSLOCTEXT("MT", "PromptOpen", "열기");

	// 프롬프트 위젯에 행동 이름 주입 (위젯 생성 지연 대비 — 가시성 타이머에서 재시도)
	void PushPromptLabel();

	// 펀치 시 열릴 콘솔 위젯 (WBP_LobbyRoomSettings / WBP_LobbyKick)
	UPROPERTY(EditDefaultsOnly, Category = "MT|Lobby")
	TSubclassOf<UUserWidget> ConsoleWidgetClass;

private:
	// 로컬 플레이어가 호스트일 때만 표시 (복제 지연 대비 주기 재평가)
	void UpdateHostVisibility();

	FTimerHandle VisibilityTimer;
};
