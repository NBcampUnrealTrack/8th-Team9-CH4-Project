#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "MTPlayerAttributeSet.h"
#include "GameFramework/Character.h"
#include "MTPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

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

	void StopJump();

	void AttractiveBeam();
	void AttractiveBeamReleased();

	void MeowPunch();

	// 스턴 중이면 스킬/이동 입력 무시
	bool IsStunned() const;

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

#pragma endregion

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilitySystem")
	TArray<TSubclassOf< UGameplayAbility>> DefaultAbilities;

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

	void Die();

private:
	// State.Stun 태그 변화 → 이동/입력 잠금 토글 (소유 클라 기준)
	void OnStunTagChanged(const FGameplayTag Tag, int32 NewCount);

	bool bStunned = false;

	UPROPERTY(Transient)
	bool bIsDead = false;
};
