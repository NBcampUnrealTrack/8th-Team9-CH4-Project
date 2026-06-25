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

	// StateTree가 3초 후 호출 → 매료 반응 종료(매료도 풀 복구, 이동 재개)
	UFUNCTION(BlueprintCallable, Category = "Attractive")
	void EndAttracted();

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

	// 행인 GE (서버/클라 공통). 매료도 초기화, 매료도 최대치 설정, 매료도 회복 주기 부여
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

	// 현재 선두 기여자 (바 색용). 기여도 테이블 전체 대신 이것만 복제.
	UPROPERTY(ReplicatedUsing = OnRep_Leader)
	TObjectPtr<APlayerState> LeadingPlayer;

	UFUNCTION()
	void OnRep_Leader();

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
	FLinearColor GetLeaderColor() const;   // AttractedBy/LeadingPlayer의 팀 색

	// 이동/배회는 StateTree(ST_Pedestrian)가 담당. 여기선 이동 속도만 설정.
	UPROPERTY(EditAnywhere, Category = "Pedestrian|Movement")
	float WalkSpeed = 180.0f;

	// 매료 시 플레이어를 향해 도는 속도
	UPROPERTY(EditAnywhere, Category = "Pedestrian|Movement")
	float AttractiveTurnSpeed = 6.0f;

	float BarVisibilityTimer = 0.f;
};
