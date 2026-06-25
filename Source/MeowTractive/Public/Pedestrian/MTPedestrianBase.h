#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Character.h"
#include "MTPedestrianBase.generated.h"

class UAbilitySystemComponent;
class UMTPedestrianAttributeSet;
class UWidgetComponent;
class UGameplayEffect;
class APlayerState;

// 매료 체력 변경 알림 (Current, Max). 클라 프로그레스바가 이벤트로 구독 → Tick 렌더링 금지.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMTOnAttractiveHealthChanged, float, AttractiveHealth, float, MaxAttractiveHealth);

// 매료된 순간. StateTree/BP가 구독해 Attracted 상태(이동 정지)로 전환.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMTOnAttracted, APlayerState*, AttractedBy);

/** 행인: HP 없음. 매료 체력(0=매료) + GAS 기반. 기여도 최다 플레이어가 매료 소유. */
UCLASS()
class MEOWTRACTIVE_API AMTPedestrianBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AMTPedestrianBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// --- AttributeSet에서 호출 (서버) ---
	void HandleAttractiveHit(APlayerState* Source, float Amount, float NewAttractiveHealth);
	void HandleAttractiveRegen(float NewAttractiveHealth);

	// 서버/클라 공통 — 델리게이트 방송 (이벤트 기반 바 갱신)
	void BroadcastAttractiveChanged();

	// --- 표시용 (BP 바인딩) ---
	UPROPERTY(BlueprintAssignable, Category = "Attractive")
	FMTOnAttractiveHealthChanged OnAttractiveHealthChanged;

	// 매료된 순간 1회 — StateTree/BP가 Attracted 상태 전환에 사용
	UPROPERTY(BlueprintAssignable, Category = "Attractive")
	FMTOnAttracted OnAttracted;

	UFUNCTION(BlueprintPure, Category = "Attractive")
	float GetAttractiveHealthValue() const;

	UFUNCTION(BlueprintPure, Category = "Attractive")
	float GetMaxAttractiveHealthValue() const;

	// 플레이어에게 보이는 "쌓인 매료도" = Max - Current
	UFUNCTION(BlueprintPure, Category = "Attractive")
	float GetAttractiveGauge() const;

	UFUNCTION(BlueprintPure, Category = "Attractive")
	bool IsAttracted() const { return bIsAttracted; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void PossessedBy(AController* NewController) override;

	// --- GAS ---
	UPROPERTY(VisibleAnywhere, Category = "Attractive|GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UMTPedestrianAttributeSet> AttributeSet;

	// 데이터 주도형: 모든 타이밍은 GE+태그로. (C++ 타이머 사용 안 함)
	UPROPERTY(EditDefaultsOnly, Category = "Attractive|GE")
	TSubclassOf<UGameplayEffect> RegenGE;           // 무한+주기, Ongoing Req: Ignore State.AttractiveInProgress

	UPROPERTY(EditDefaultsOnly, Category = "Attractive|GE")
	TSubclassOf<UGameplayEffect> AttractiveInProgressGE; // 7s, grants State.AttractiveInProgress, 적용 시 Duration Refresh

	UPROPERTY(EditDefaultsOnly, Category = "Attractive|GE")
	TSubclassOf<UGameplayEffect> InvulnerableGE;    // 15s, grants State.Invulnerable

	// 매료 소유자 (복제). 기여도 테이블 전체가 아닌 결과만 동기화.
	UPROPERTY(ReplicatedUsing = OnRep_Attracted)
	TObjectPtr<APlayerState> AttractedBy;

	UFUNCTION()
	void OnRep_Attracted();

	UPROPERTY(VisibleAnywhere, Category = "Attractive|UI")
	TObjectPtr<UWidgetComponent> AttractiveBarWidget;

	UPROPERTY(EditAnywhere, Category = "Attractive|UI")
	float AttractiveBarVisibleDistance = 1500.f;

private:
	// 기여도 테이블 — 약참조 키(튕김/접속종료 대비). 서버 전용, 복제 안 함.
	TMap<TWeakObjectPtr<APlayerState>, float> AttractiveContributions;
	TWeakObjectPtr<APlayerState> LastAttacker;

	bool bIsAttracted = false;

	// 캐시 태그
	FGameplayTag InvulnerableTag;
	FGameplayTag AttractiveInProgressTag;

	// 기여도 최다(동점=라스트힛) 산출
	APlayerState* DetermineAttractiveWinner() const;
	void BecomeAttracted(APlayerState* Winner);
	void ResetContributions();

	void UpdateAttractiveBarVisibility();

	// 이동/배회는 StateTree(ST_Pedestrian)가 담당. 여기선 이동 속도만 설정.
	UPROPERTY(EditAnywhere, Category = "Pedestrian|Movement")
	float WalkSpeed = 180.0f;

	// 매료 시 플레이어를 향해 도는 속도
	UPROPERTY(EditAnywhere, Category = "Pedestrian|Movement")
	float AttractiveTurnSpeed = 6.0f;

	float BarVisibilityTimer = 0.f;
};
