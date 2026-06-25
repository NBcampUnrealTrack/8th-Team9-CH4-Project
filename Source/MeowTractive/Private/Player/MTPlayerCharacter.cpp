#include "Player/MTPlayerCharacter.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Game/MTGameplayTags.h"

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

	AttributeSet = CreateDefaultSubobject<UMTPlayerAttributeSet>(TEXT("AttributeSetBase"));
}

void AMTPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocallyControlled())
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			PC->SetInputMode(FInputModeGameOnly());
			PC->SetShowMouseCursor(false);
		}
	}
}

void AMTPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 점프 테스트
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	const bool bCIsDown = PC->IsInputKeyDown(EKeys::C);

	// 막 눌린 순간 → 점프 시작
	if (bCIsDown && !bWasCJumpHeld)
	{
		Jump();
	}

	// 막 떼진 순간 → 아직 올라가는 중이면 상승 끊기
	if (!bCIsDown && bWasCJumpHeld)
	{
		FVector Vel = GetCharacterMovement()->Velocity;
		if (Vel.Z > 0.f)
		{
			Vel.Z *= 0.4f; // 남은 상승 속도를 깎아서 정점 낮춤
			GetCharacterMovement()->Velocity = Vel;
		}
	}

	bWasCJumpHeld = bCIsDown;
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
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AMTPlayerCharacter::StopJumping);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMTPlayerCharacter::Move);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMTPlayerCharacter::Look);
		EnhancedInputComponent->BindAction(DashAction, ETriggerEvent::Started, this, &AMTPlayerCharacter::Dash);
		EnhancedInputComponent->BindAction(AttractiveBeamAction, ETriggerEvent::Started, this, &AMTPlayerCharacter::AttractiveBeam);
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
}

void AMTPlayerCharacter::Move(const FInputActionValue& Value)
{
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
	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->TryActivateAbilitiesByTag(
		FGameplayTagContainer(MTGameplayTags::Ability::TAG_Skill_Move_Dash), true);
}

void AMTPlayerCharacter::AttractiveBeam()
{
	if (!AbilitySystemComponent)
	{
		return;
	};

	AbilitySystemComponent->TryActivateAbilitiesByTag(
		FGameplayTagContainer(MTGameplayTags::Ability::TAG_Skill_Attract_Beam), true);
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
