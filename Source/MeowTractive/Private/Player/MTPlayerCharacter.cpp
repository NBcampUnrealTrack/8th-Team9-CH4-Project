#include "Player/MTPlayerCharacter.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Game/MTGameplayTags.h"
#include "Game/MTLog.h"
#include "Player/MTPlayerState.h"
#include "Engine/Engine.h"

AMTPlayerCharacter::AMTPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	JumpMaxCount = 2;

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	// ASC가 Pawn에 있으므로 MP에선 Full (Mixed는 PlayerState 소유 전제)
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Full);

	AttributeSet = CreateDefaultSubobject<UMTPlayerAttributeSet>(TEXT("AttributeSetBase"));
}

void AMTPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	const bool bLocally = IsLocallyControlled();
	APlayerController* PC = Cast<APlayerController>(GetController());

	// 소유 클라 포함 모든 머신에서 ActorInfo 세팅 (LocalPredicted 어빌리티/속성·태그 복제 동작).
	// 서버는 PossessedBy에서도 호출하지만 반복 호출 안전.
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		AbilitySystemComponent->RegisterGameplayTagEvent(
			MTGameplayTags::State::TAG_State_Stun, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &AMTPlayerCharacter::OnStunTagChanged);
	}

	// 클라는 보통 여기서 적용됨. 호스트(seamless)는 아직 미possess라 NotifyControllerChanged에서 적용.
	ApplyLocalInputMode();

	if (MTLogEnabled())
	{
		// 호스트 입력잠김 진단: 이 분기를 탔는지 / 컨트롤러가 BeginPlay 시점에 붙었는지
		const FString Msg = FString::Printf(TEXT("[MTPawn] BeginPlay LocallyCtrl=%d PC=%s Auth=%d NetMode=%d 입력모드적용=%d | Pawn=%s"),
			bLocally ? 1 : 0,
			PC ? *PC->GetName() : TEXT("null"),
			HasAuthority() ? 1 : 0,
			(int32)GetNetMode(),
			(bLocally && PC) ? 1 : 0,
			*GetName());
		UE_LOG(LogMT, Log, TEXT("%s"), *Msg);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Green, Msg);
	}
}

void AMTPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AMTPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	AbilitySystemComponent->InitAbilityActorInfo(this,this);

	if (HasAuthority() && AbilitySystemComponent)
	{
		for (const TSubclassOf<UGameplayAbility>& Ability : DefaultAbilities)
		{
			if (Ability)
			{
				AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(Ability, 1, INDEX_NONE, this));
			}
		}
	}
}

void AMTPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {

		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AMTPlayerCharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AMTPlayerCharacter::StopJump);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMTPlayerCharacter::Move);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMTPlayerCharacter::Look);
		EnhancedInputComponent->BindAction(DashAction, ETriggerEvent::Started, this, &AMTPlayerCharacter::Dash);
		EnhancedInputComponent->BindAction(AttractiveBeamAction, ETriggerEvent::Started, this, &AMTPlayerCharacter::AttractiveBeam);
		EnhancedInputComponent->BindAction(AttractiveBeamAction, ETriggerEvent::Completed, this, &AMTPlayerCharacter::AttractiveBeamReleased);
		EnhancedInputComponent->BindAction(MeowPunchAction, ETriggerEvent::Started, this, &AMTPlayerCharacter::MeowPunch);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AMTPlayerCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	// possession 직후 — 호스트(seamless travel)는 여기서 게임 입력모드 확정
	ApplyLocalInputMode();
}

void AMTPlayerCharacter::ApplyLocalInputMode()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC && PC->IsLocalController())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);

		if (MTLogEnabled())
		{
			const FString Msg = FString::Printf(TEXT("[MTPawn] ApplyLocalInputMode → GameOnly | PC=%s NetMode=%d Pawn=%s"),
				*PC->GetName(), (int32)GetNetMode(), *GetName());
			UE_LOG(LogMT, Log, TEXT("%s"), *Msg);
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Magenta, Msg);
		}
	}
}

void AMTPlayerCharacter::Move(const FInputActionValue& Value)
{
	if (IsStunned())
	{
		return;
	}

	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AMTPlayerCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void AMTPlayerCharacter::Dash()
{
	if (!AbilitySystemComponent || IsStunned())
	{
		return;
	}

	AbilitySystemComponent->TryActivateAbilitiesByTag(
		FGameplayTagContainer(MTGameplayTags::Ability::TAG_Skill_Move_Dash), true);
}

void AMTPlayerCharacter::MeowPunch()
{
	if (!AbilitySystemComponent || IsStunned())
	{
		return;
	}

	AbilitySystemComponent->TryActivateAbilitiesByTag(
		FGameplayTagContainer(MTGameplayTags::Ability::TAG_Skill_Attack_MeowPunch), true);
}

void AMTPlayerCharacter::StopJump()
{
	StopJumping();

	// 가변 점프: 점프 키를 일찍 떼면 남은 상승 속도를 깎아 정점을 낮춤
	FVector Vel = GetCharacterMovement()->Velocity;
	if (Vel.Z > 0.f)
	{
		Vel.Z *= 0.4f;
		GetCharacterMovement()->Velocity = Vel;
	}
}

void AMTPlayerCharacter::AttractiveBeam()
{
	if (!AbilitySystemComponent || IsStunned())
	{
		return;
	};

	AbilitySystemComponent->TryActivateAbilitiesByTag(
		FGameplayTagContainer(MTGameplayTags::Ability::TAG_Skill_Attract_Beam), true);
}

void AMTPlayerCharacter::AttractiveBeamReleased()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	FGameplayTagContainer BeamTags(MTGameplayTags::Ability::TAG_Skill_Attract_Beam);
	AbilitySystemComponent->CancelAbilities(&BeamTags);
}


void AMTPlayerCharacter::ApplyDamagePlayer(float DamageAmount)
{
	float NewHp = AttributeSet->GetHp() - DamageAmount;
	AttributeSet->SetHp(NewHp);
}

UAbilitySystemComponent* AMTPlayerCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

bool AMTPlayerCharacter::IsStunned() const
{
	return bStunned;
}

bool AMTPlayerCharacter::IsEnemyCat(const AActor* SourceActor, const AActor* TargetActor)
{
	const AMTPlayerCharacter* Src = Cast<AMTPlayerCharacter>(SourceActor);
	const AMTPlayerCharacter* Tgt = Cast<AMTPlayerCharacter>(TargetActor);
	if (!Src || !Tgt || Src == Tgt)
	{
		return false;
	}

	const AMTPlayerState* SrcPS = Src->GetPlayerState<AMTPlayerState>();
	const AMTPlayerState* TgtPS = Tgt->GetPlayerState<AMTPlayerState>();
	if (!SrcPS || !TgtPS)
	{
		return false;
	}

	const int32 SrcTeam = SrcPS->GetTeamId();
	const int32 TgtTeam = TgtPS->GetTeamId();
	// 개인전(TeamId<0): 자기 외 전원 적. 팀전: 다른 팀만 적.
	if (SrcTeam < 0 || TgtTeam < 0)
	{
		return true;
	}
	return SrcTeam != TgtTeam;
}

void AMTPlayerCharacter::OnStunTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	// 이동/입력 잠금은 조작하는(소유) 클라 기준으로만 — 이동 권위가 클라에 있음
	if (!IsLocallyControlled())
	{
		return;
	}

	bStunned = NewCount > 0;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		if (bStunned)
		{
			Move->StopMovementImmediately();
			Move->DisableMovement();
			StopJumping();
			// 시전 중이던 스킬(빔 등) 중단
			if (AbilitySystemComponent)
			{
				AbilitySystemComponent->CancelAllAbilities();
			}
		}
		else
		{
			Move->SetMovementMode(MOVE_Walking);
		}
	}
}
