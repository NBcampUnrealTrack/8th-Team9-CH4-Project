#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Player/MTLobbySelectable.h"
#include "Game/MTTypes.h"
#include "MTLobbyCharacter.generated.h"

class UWidgetComponent;

/**
 * 로비 맵에 배치하는 고양이 선택 조형물 (개인화).
 * ACharacter 직계 — AMTPlayerCharacter와 분리 (카메라·입력·GAS 없음, 매치로 넘어가지 않음).
 * OwnerSlot(플레이어 슬롯)로 소유자를 구분해, 그 슬롯 플레이어에게만 보이고 그만 펀치할 수 있다.
 * 펀치 시 NextCatActorClass로 교체 스폰되고(슬롯 승계), 때린 플레이어의 SelectedCat도 갱신된다.
 * 각 고양이(점박이/고등어/뚱냥이)는 이 클래스의 BP 서브클래스로 만든다. 로비 GameMode에서만 동작.
 */
UCLASS(Abstract, Blueprintable)
class MEOWTRACTIVE_API AMTLobbyCharacter : public ACharacter, public IMTLobbySelectable
{
	GENERATED_BODY()

public:
	AMTLobbyCharacter();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 서버: 펀치 히트 → (소유자 검증) 다음 BP 스폰 + 선택 갱신 + 자기 파괴
	virtual void OnPunchSelect(AActor* InstigatorPawn) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 소유 슬롯 기준 가시성 (내 슬롯 조형물만 표시)
	virtual void UpdateLobbyVisibility();

	// 이 조형물이 나타내는 고양이 (선택에 반영되는 값)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MT|Lobby")
	EMTCatType RepresentedCat = EMTCatType::Spotted;

	// 펀치 시 교체될 다음 조형물 BP (예: BP_Spotted→BP_Mackerel→BP_Fatty→BP_Spotted)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MT|Lobby")
	TSubclassOf<AMTLobbyCharacter> NextCatActorClass;

	// 이 조형물의 소유 플레이어 슬롯 (배치본은 에디터 지정, 교체본은 승계). 그 슬롯 플레이어만 보고/펀치.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "MT|Lobby")
	int32 OwnerSlot = -1;

	// "C키로 선택" 프롬프트 (WidgetClass는 BP에서 지정)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MT|Lobby")
	TObjectPtr<UWidgetComponent> PromptWidget;

private:
	FTimerHandle VisibilityTimer;   // 로컬 슬롯/복제 지연 대비 주기 재평가
};
