#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Player/MTLobbySelectable.h"
#include "Game/MTTypes.h"
#include "MTLobbyCharacter.generated.h"

class UWidgetComponent;
class UMaterialInterface;

// 선택 사이클 한 항목 — 표시 고양이와 그 머티리얼
USTRUCT(BlueprintType)
struct FMTLobbyCatOption
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MT|Lobby")
	EMTCatType Cat = EMTCatType::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MT|Lobby")
	TObjectPtr<UMaterialInterface> Material;
};

/**
 * 로비 맵에 배치하는 고양이 선택 조형물 (개인화).
 * 세 고양이가 같은 메시에 머티리얼만 다르므로, 펀치 시 액터 교체 없이
 * 복제 인덱스(CatIndex)만 순환시키고 머티리얼을 갈아끼운다 — 스폰/파괴가 없어
 * 중복·유령 조형물 계열 버그가 원천 차단된다. 로비 GameMode에서만 동작.
 */
UCLASS(Abstract, Blueprintable)
class MEOWTRACTIVE_API AMTLobbyCharacter : public ACharacter, public IMTLobbySelectable
{
	GENERATED_BODY()

public:
	AMTLobbyCharacter();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 서버: 펀치 히트 → (소유자 검증) 보이던 고양이 선택 + 다음 고양이로 머티리얼 순환
	virtual void OnPunchSelect(AActor* InstigatorPawn) override;

	// 현재 표시 중인 고양이
	EMTCatType GetRepresentedCat() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 소유 슬롯 기준 가시성 (내 슬롯 조형물만 표시)
	virtual void UpdateLobbyVisibility();

	// 선택 사이클 (순서 = 펀치 순환 순서). C++ 기본값 제공 — BP 세팅 불필요, 필요 시 오버라이드.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MT|Lobby")
	TArray<FMTLobbyCatOption> CatOptions;

	// 배치 시 초기 표시 고양이 (서버 셔플이 덮어씀). 기존 BP 호환용.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MT|Lobby")
	EMTCatType RepresentedCat = EMTCatType::Spotted;

	// 이 조형물의 소유 플레이어 슬롯 (에디터 지정). 그 슬롯 플레이어만 보고/펀치.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "MT|Lobby")
	int32 OwnerSlot = -1;

	// "[키] 행동" 프롬프트 (WidgetClass는 BP에서 지정 — WBP_Select)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MT|Lobby")
	TObjectPtr<UWidgetComponent> PromptWidget;

	// 프롬프트에 표시할 행동 이름 ("[C] 고양이 선택")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MT|Lobby")
	FText PromptLabel = NSLOCTEXT("MT", "PromptSelectCat", "고양이 선택");

	// 프롬프트 위젯에 행동 이름 주입 (위젯 생성 지연 대비 — 가시성 타이머에서 재시도)
	void PushPromptLabel();

	// 근접 아웃라인 트리거 (Pawn 오버랩)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MT|Lobby")
	TObjectPtr<class USphereComponent> OutlineTrigger;

	// 근접 아웃라인 색 (MM_PPInteractOutline 스텐실 1~3)
	UPROPERTY(EditAnywhere, Category = "MT|Lobby", meta = (ClampMin = "0", ClampMax = "255"))
	int32 OutlineStencil = 2;

private:
	// OutlineTrigger 진입/이탈 → 로컬 플레이어에게 아웃라인 표시
	UFUNCTION()
	void OnOutlineTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOutlineTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// 현재 표시 중인 사이클 인덱스 (서버 확정 → 전 클라 복제)
	UPROPERTY(ReplicatedUsing = OnRep_CatIndex)
	int32 CatIndex = INDEX_NONE;

	UFUNCTION()
	void OnRep_CatIndex();

	// CatIndex의 머티리얼을 메시 전 슬롯에 적용
	void ApplyCatMaterial();

	FTimerHandle VisibilityTimer;   // 로컬 슬롯/복제 지연 대비 주기 재평가
};
