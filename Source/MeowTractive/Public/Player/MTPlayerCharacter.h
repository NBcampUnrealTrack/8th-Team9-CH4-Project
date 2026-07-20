#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "MTPlayerAttributeSet.h"
#include "Game/MTTypes.h"
#include "GameFramework/Character.h"
#include "MTPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UNiagaraComponent;
class UNiagaraSystem;
class UMaterialInterface;
struct FInputActionValue;

// 어빌리티 입력 슬롯 (FGameplayAbilitySpec.InputID). 고양이별로 다른 스킬이 같은 슬롯에 매핑됨.
UENUM(BlueprintType)
enum class EMTAbilitySlot : uint8
{
	SkillA = 0, // 주 스킬 (예: Q)
	SkillB = 1, // 보조 스킬 (예: E)
};

// 종류별 전용 어빌리티 묶음 (TMap 값). 슬롯 명시형 — 입력은 슬롯만 알고 실제 스킬은 데이터로 갈림.
USTRUCT(BlueprintType)
struct FMTCatAbilitySet
{
	GENERATED_BODY()

	// SkillA 슬롯에 바인딩 (InputID = EMTAbilitySlot::SkillA)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilitySystem")
	TSubclassOf<UGameplayAbility> SkillA;

	// SkillB 슬롯에 바인딩 (InputID = EMTAbilitySlot::SkillB)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilitySystem")
	TSubclassOf<UGameplayAbility> SkillB;

	// 입력 없이 Grant (자동 발동 패시브 등)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilitySystem")
	TArray<TSubclassOf<UGameplayAbility>> Passives;
};

UCLASS()
class MEOWTRACTIVE_API AMTPlayerCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

private:
	bool bWasCJumpHeld = false; // 점프 테스트

	// 로컬 컨트롤러 소유 시 게임 입력모드 적용. seamless travel 시 호스트는 BeginPlay가
	// possession보다 먼저라, possession 직후(NotifyControllerChanged)에서도 호출해야 한다.
	void ApplyLocalInputMode();

protected:
	AMTPlayerCharacter();

	void Move(const FInputActionValue& Value);

	void Look(const FInputActionValue& Value);

	void Dash();

	virtual void Jump() override;

	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	USoundBase* JumpSound;

	void StopJump();

	virtual void Landed(const FHitResult& Hit) override;

	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	USoundBase* LandingSound;

	void UpdateMovementEffects();

	FVector GetMovementEffectLocation() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects|Movement", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> JumpNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects|Movement", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> LandingNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects|Movement", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> WalkNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects|Movement", meta = (AllowPrivateAccess = "true"))
	FVector MovementEffectOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects|Movement", meta = (AllowPrivateAccess = "true"))
	FName WalkDirectionParameterName = TEXT("User.Direction");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects|Movement", meta = (AllowPrivateAccess = "true"))
	FName WalkSpawnParameterName = TEXT("User.ShouldSpawn");

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ActiveWalkNiagaraComponent;

	void AttractiveBeam();
	void AttractiveBeamReleased();

	void MeowPunch();

	// 로비에서만 R키 → 준비 토글 (매치에선 무시). 서버 RPC는 컨트롤러가 처리.
	void ToggleReady();

	void Interact();

	// 종류별 스킬 슬롯: 입력은 슬롯 InputID만 GAS에 전달, 실제 어빌리티는 CatAbilities가 결정
	void OnSkillAPressed();
	void OnSkillAReleased();
	void OnSkillBPressed();
	void OnSkillBReleased();

	// 스턴 중이면 스킬/이동 입력 무시
	bool IsStunned() const;

	// 로비에서만: 남의 폰은 실루엣, 내 폰은 원본. 소유 판정 확정마다(BeginPlay·possession) 재적용해 양방향 보정. 매치에선 무효.
	virtual void ApplyLobbyDisguise();

	// 로비에서 남의 폰에 덮어씌울 머티리얼. 세 고양이가 메시를 공유하므로 이것만으로 선택이 가려진다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MT|Lobby")
	TObjectPtr<UMaterialInterface> LobbyDisguiseMaterial;

	// 실루엣 복원용 원본 머티리얼 캐시 (BeginPlay에서 덮이기 전 1회 저장).
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> OriginalMaterials;

	// 사망 애니메이션
	UPROPERTY(EditDefaultsOnly, Category = "Die")
	TArray<UAnimMontage*> DeathMontages; // R, L 등록

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayDeathMontage(UAnimMontage* MontageToPlay);
#pragma region PlayerInput

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* DashAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* AttractiveBeamAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* MeowPunchAction;

	// 종류별 스킬 슬롯 (고양이마다 실제 스킬 다름)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* SkillAAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* SkillBAction;

	// 로비 준비 토글 (R). 로비 전용 — 매치에선 핸들러가 무시
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* ReadyAction;

	// 로비 상호작용 (F) → GA_Interact
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* InteractAction;

#pragma endregion

	// 전 고양이 공통 부여 어빌리티
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilitySystem")
	TArray<TSubclassOf< UGameplayAbility>> DefaultAbilities;

	// 종류별 전용 어빌리티: PossessedBy에서 PlayerState의 SelectedCat에 맞는 세트만 Grant
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilitySystem")
	TMap<EMTCatType, FMTCatAbilitySet> CatAbilities;

	// 로비 선택이 없을 때(단독 PIE/테스트) 사용할 폴백 고양이. None이면 폴백 없음.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilitySystem")
	EMTCatType DefaultCatType = EMTCatType::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AbilitySystem")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

public:
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	virtual void NotifyControllerChanged() override;

	virtual void PossessedBy(AController* NewController) override;

	UFUNCTION(BlueprintCallable)
	void ApplyDamagePlayer(float DamageAmount);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AbilitySystem")
	TObjectPtr<UMTPlayerAttributeSet> AttributeSet;

	// 근접/대시 데미지 대상 판정: 서로 다른 고양이 + 적팀(개인전은 자기 외 전원 적)
	static bool IsEnemyCat(const AActor* SourceActor, const AActor* TargetActor);

	// 매치 종료: 진행 중 스킬 취소 + 이동 정지 (결과 화면 동안 행동 지속 방지). 서버 전용
	void FreezeForMatchEnd();

	// 현재 선택 고양이의 대표 패시브 클래스 (HUD 패시브 슬롯용). PossessedBy의 부여 로직과 동일 기준.
	TSubclassOf<UGameplayAbility> GetActivePassiveClass() const;

	// 피격 큐(GC_CatHit) 등에서 사망자 연출 스킵용 — BP 노출
	UFUNCTION(BlueprintPure, Category = "MT|State")
	bool IsDead() const { return bIsDead; }

	void Die(AController* KillerController = nullptr);

	// 로비 등에서 마우스 커서/UI 입력 토글 (강퇴·메뉴 클릭용). 로컬 전용.
	// bEnable=true: GameAndUI + 커서 표시 / false: GameOnly + 커서 숨김
	UFUNCTION(BlueprintCallable, Category = "MT|Input")
	void SetUICursorEnabled(bool bEnable);

	// 시전 중인 액티브 스킬(Skill.*) 전부 취소 — 메뉴/콘솔 열 때 호출.
	// UIOnly 전환 시 뷰포트가 눌린 키의 release를 합성해 홀드형 스킬이 발사되는 것 방지.
	UFUNCTION(BlueprintCallable, Category = "MT|Ability")
	void CancelActiveSkills();

	UFUNCTION(BlueprintCallable, Category = "MT|Effects|Movement")
	UNiagaraComponent* SpawnWalkNiagara();

	UFUNCTION(BlueprintCallable, Category = "MT|Effects|Movement")
	void StopWalkNiagara();

	// 서버(GA_Dash): 대시 재충전 진행 기록 (EndServerTime=0이면 재충전 없음). HUD 표시용
	void SetDashRechargeState(float EndServerTime, float Duration);

	UFUNCTION(BlueprintPure, Category = "MT|UI")
	float GetDashRechargeEndServerTime() const { return DashRechargeEndServerTime; }

	UFUNCTION(BlueprintPure, Category = "MT|UI")
	float GetDashRechargeDuration() const { return DashRechargeDuration; }

private:
	// State.Stun 태그 변화 → 이동/입력 잠금 토글 (소유 클라 기준)
	void OnStunTagChanged(const FGameplayTag Tag, int32 NewCount);

	// MoveSpeedMult 어트리뷰트 변화 → 속도 재계산 (패시브/감속 GE 공통 경로)
	void OnMoveSpeedMultChanged(const FOnAttributeChangeData& Data);

	// 최종 속도 = BaseWalkSpeed × MoveSpeedMult
	void RecalcMoveSpeed();

	// 슬로우 복구 기준 원본 속도 (BeginPlay 캐시)
	float BaseWalkSpeed = 500.f;

	bool bStunned = false;

	UPROPERTY(Transient, ReplicatedUsing = OnRep_IsDead)
	bool bIsDead = false;

	// 다음 대시 충전 완료 서버시각 (GetServerWorldTimeSeconds 기준, 0=재충전 없음)
	UPROPERTY(Replicated)
	float DashRechargeEndServerTime = 0.f;

	// 충전 1회 재충전 시간 — HUD 진행률 계산용
	UPROPERTY(Replicated)
	float DashRechargeDuration = 0.f;

	UFUNCTION()
	void OnRep_IsDead();
};
