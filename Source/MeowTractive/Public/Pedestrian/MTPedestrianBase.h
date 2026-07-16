#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Character.h"
#include "Pedestrian/MTPedestrianAppearanceTypes.h"
#include "MTPedestrianBase.generated.h"

class UAbilitySystemComponent;
class UMTPedestrianAttributeSet;
class UMTAttractiveComponent;
class UWidgetComponent;
class UGameplayEffect;
class UNiagaraSystem;
class APlayerState;

// 최고 매료 수치 변경 알림 (Current, Max). 클라 프로그레스바가 이벤트로 구독.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMTOnAttractiveAmountChanged, float, AttractiveAmount, float, MaxAttractiveAmount);

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

	void SetAppearance(const FMTPedestrianAppearance& NewAppearance);

	UFUNCTION(BlueprintPure, Category = "Pedestrian")
	const FMTPedestrianAppearance& GetAppearance() const { return Appearance; }

	// --- AttributeSet에서 호출 (서버) ---
	void HandleAttractiveHit(APlayerState* Source, float Amount);
	void HandleAttractiveRegen(float Amount);

	// 서버/클라 공통 — 델리게이트 방송 (이벤트 기반 바 갱신)
	void BroadcastAttractiveChanged();
	void HandleAttractiveAmountsChanged();

	// 매치 종료 시 AI 정지 (서버 전용). 서버에서 멈추면 이동 복제로 전 클라에 반영됨.
	void FreezeForMatchEnd();

	// --- 표시용 (BP 바인딩) ---
	UPROPERTY(BlueprintAssignable, Category = "Attractive")
	FMTOnAttractiveAmountChanged OnAttractiveAmountChanged;

	// 매료된 순간 1회 — StateTree/BP가 Attracted 상태 전환에 사용
	UPROPERTY(BlueprintAssignable, Category = "Attractive")
	FMTOnAttracted OnAttracted;

	// StateTree가 3초 후 호출 → 매료 반응 종료(매료도 풀 복구, 이동 재개)
	UFUNCTION(BlueprintCallable, Category = "Attractive")
	void EndAttracted();

	UFUNCTION(BlueprintPure, Category = "Attractive")
	float GetAttractiveAmountValue(APlayerState* TargetPlayerState) const;

	UFUNCTION(BlueprintPure, Category = "Attractive")
	float GetHighestAttractiveAmountValue() const;

	UFUNCTION(BlueprintPure, Category = "Attractive")
	float GetMaxAttractiveAmountValue() const;

	UFUNCTION(BlueprintPure, Category = "Attractive")
	UMTAttractiveComponent* GetAttractiveComponent() const { return AttractiveComponent; }

	UFUNCTION(BlueprintPure, Category = "Attractive")
	bool IsAttracted() const { return bIsAttracted; }

	UPROPERTY(EditDefaultsOnly, Category = "SFX")
	USoundBase* AttractionTickSound;

	UPROPERTY(EditDefaultsOnly, Category = "SFX")
	USoundBase* AttractionCompleteSound;

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayAttractionTickSFX();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayAttractionCompleteSFX();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void PossessedBy(AController* NewController) override;

	// --- GAS ---
	UPROPERTY(VisibleAnywhere, Category = "Attractive|GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	//AT는 GAS공격에서의 데미지를 중계해주는 용도로만 사용
	UPROPERTY()
	TObjectPtr<UMTPedestrianAttributeSet> AttributeSet;

	//실제 매료도 저장/관리 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attractive", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMTAttractiveComponent> AttractiveComponent;

	// 행인 GE (서버/클라 공통). 매료도 초기화, 매료도 최대치 설정, 매료도 회복 주기 부여
	UPROPERTY(EditDefaultsOnly, Category = "Attractive|GE")
	TSubclassOf<UGameplayEffect> RegenGE;           // 무한+주기, 플레이어별 감소 가능 여부는 Component가 판정

	UPROPERTY(EditDefaultsOnly, Category = "Attractive|GE")
	TSubclassOf<UGameplayEffect> InvulnerableGE;    // 15s, grants State.Invulnerable

	UPROPERTY(EditDefaultsOnly, Category = "Attractive|FX")
	TObjectPtr<UNiagaraSystem> AttractedFX;

	UPROPERTY(EditDefaultsOnly, Category = "Attractive|FX")
	FVector AttractedFXOffset = FVector(0.f, 0.f, 50.f);

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

	UPROPERTY(ReplicatedUsing = OnRep_Appearance, VisibleInstanceOnly, Category = "Pedestrian")
	FMTPedestrianAppearance Appearance;

	UFUNCTION()
	void OnRep_Appearance();

private:
	bool bIsAttracted = false;

	// 캐시 태그
	FGameplayTag InvulnerableTag;

	void BecomeAttracted(APlayerState* Winner);
	void ResetAttractiveAmounts();
	void UpdateLeadingPlayer();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayAttractedEffect(FLinearColor PlayerColor);

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
