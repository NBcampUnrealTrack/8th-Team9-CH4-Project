#include "Player/MTPlayerCharacter.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Game/MTGameplayTags.h"
#include "Game/MTLog.h"
#include "Player/MTPlayerState.h"
#include "Player/MTPlayerController.h"
#include "Engine/Engine.h"
#include "Game/MTMatchGameMode.h"
#include "Game/MTLobbyGameMode.h"
#include "Game/MTLobbyGameState.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

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

	// 로비 실루엣용 (BP_MTPlayerCharacter 상속 → 전 고양이 공통). BP에서 교체 가능.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DisguiseMat(
		TEXT("/Game/Cat_Simple/Cat/Materials/M_CatSimple_C1"));
	if (DisguiseMat.Succeeded())
	{
		LobbyDisguiseMaterial = DisguiseMat.Object;
	}
}

void AMTPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	const bool bLocally = IsLocallyControlled();
	APlayerController* PC = Cast<APlayerController>(GetController());

	// 실루엣 복원용 원본 머티리얼 캐시 — 덮이기 전(BeginPlay 시작)에 1회 저장
	if (OriginalMaterials.Num() == 0)
	{
		if (const USkeletalMeshComponent* MeshComp = GetMesh())
		{
			OriginalMaterials.Reset();
			for (UMaterialInterface* Mat : MeshComp->GetMaterials())
			{
				OriginalMaterials.Add(Mat);
			}
		}
	}

	// 소유 클라 포함 모든 머신에서 ActorInfo 세팅 (LocalPredicted 어빌리티/속성·태그 복제 동작).
	// 서버는 PossessedBy에서도 호출하지만 반복 호출 안전.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		BaseWalkSpeed = Move->MaxWalkSpeed; // 슬로우 복구 기준
	}

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		AbilitySystemComponent->RegisterGameplayTagEvent(
			MTGameplayTags::State::TAG_State_Stun, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &AMTPlayerCharacter::OnStunTagChanged);
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UMTPlayerAttributeSet::GetMoveSpeedMultAttribute())
			.AddUObject(this, &AMTPlayerCharacter::OnMoveSpeedMultChanged);
	}

	// 클라는 보통 여기서 적용됨. 호스트(seamless)는 아직 미possess라 NotifyControllerChanged에서 적용.
	ApplyLocalInputMode();
	ApplyLobbyDisguise();   // 로비: 남의 폰 실루엣 처리 (매치선 무효)

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
	UpdateMovementEffects();
}

void AMTPlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMTPlayerCharacter, bIsDead);
	DOREPLIFETIME(AMTPlayerCharacter, DashRechargeEndServerTime);
	DOREPLIFETIME(AMTPlayerCharacter, DashRechargeDuration);
}

void AMTPlayerCharacter::CancelActiveSkills()
{
	if (AbilitySystemComponent)
	{
		// 액티브 스킬만 취소 (패시브는 Skill.* 에셋 태그가 없어 영향 없음)
		FGameplayTagContainer SkillTags(FGameplayTag::RequestGameplayTag(TEXT("Skill")));
		AbilitySystemComponent->CancelAbilities(&SkillTags);
	}
}

void AMTPlayerCharacter::SetDashRechargeState(float EndServerTime, float Duration)
{
	if (!HasAuthority())
	{
		return;
	}
	DashRechargeEndServerTime = EndServerTime;
	DashRechargeDuration = Duration;
}

void AMTPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	AbilitySystemComponent->InitAbilityActorInfo(this,this);

	if (HasAuthority() && AbilitySystemComponent)
	{
		// 공통 어빌리티
		for (const TSubclassOf<UGameplayAbility>& Ability : DefaultAbilities)
		{
			if (Ability)
			{
				AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(Ability, 1, INDEX_NONE, this));
			}
		}

		// 종류별 전용 (선택 고양이 기준). PlayerState는 Super::PossessedBy에서 세팅됨.
		// 액티브는 슬롯 InputID로, 패시브는 InputID 없이 Grant.
		EMTCatType Cat = EMTCatType::None;
		if (const AMTPlayerState* PS = GetPlayerState<AMTPlayerState>())
		{
			Cat = PS->GetSelectedCat();
		}
		if (Cat == EMTCatType::None)
		{
			Cat = DefaultCatType; // 로비 선택 없을 때 폴백
		}

		// 로비에선 Q/E 액티브 스킬 미부여 (이동·펀치로 조작만 연습). 패시브는 그대로.
		const bool bInLobby = GetWorld() && GetWorld()->GetGameState<AMTLobbyGameState>();

		if (const FMTCatAbilitySet* Set = CatAbilities.Find(Cat))
		{
			if (Set->SkillA && !bInLobby)
			{
				AbilitySystemComponent->GiveAbility(
					FGameplayAbilitySpec(Set->SkillA, 1, (int32)EMTAbilitySlot::SkillA, this));
			}
			if (Set->SkillB && !bInLobby)
			{
				AbilitySystemComponent->GiveAbility(
					FGameplayAbilitySpec(Set->SkillB, 1, (int32)EMTAbilitySlot::SkillB, this));
			}
			for (const TSubclassOf<UGameplayAbility>& Passive : Set->Passives)
			{
				if (Passive)
				{
					AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(Passive, 1, INDEX_NONE, this));
				}
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

		// 로비 준비 토글 (R) — 미할당 BP 대비 널가드
		if (ReadyAction)
		{
			EnhancedInputComponent->BindAction(ReadyAction, ETriggerEvent::Started, this, &AMTPlayerCharacter::ToggleReady);
		}

		// 로비 상호작용 (F) — 미할당 BP 대비 널가드
		if (InteractAction)
		{
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &AMTPlayerCharacter::Interact);
		}

		// 슬롯 입력: 눌림/뗌 모두 GAS에 전달 (홀드형 스킬도 슬롯만으로 지원)
		EnhancedInputComponent->BindAction(SkillAAction, ETriggerEvent::Started, this, &AMTPlayerCharacter::OnSkillAPressed);
		EnhancedInputComponent->BindAction(SkillAAction, ETriggerEvent::Completed, this, &AMTPlayerCharacter::OnSkillAReleased);
		EnhancedInputComponent->BindAction(SkillBAction, ETriggerEvent::Started, this, &AMTPlayerCharacter::OnSkillBPressed);
		EnhancedInputComponent->BindAction(SkillBAction, ETriggerEvent::Completed, this, &AMTPlayerCharacter::OnSkillBReleased);
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
			// bNotifyUserSettings: 저장된 키 재바인딩(EnhancedInputUserSettings) 적용
			FModifyContextOptions Options;
			Options.bNotifyUserSettings = true;
			Subsystem->AddMappingContext(DefaultMappingContext, 0, Options);
		}
	}

	// possession 직후 — 호스트(seamless travel)는 여기서 게임 입력모드 확정
	ApplyLocalInputMode();
	ApplyLobbyDisguise();   // possession 확정 후 내/남 폰 판정 재적용
}

void AMTPlayerCharacter::FreezeForMatchEnd()
{
	if (!HasAuthority())
	{
		return;
	}

	// 진행 중 스킬 전부 종료 — 결과 화면이 UIOnly로 바뀌면 홀드 키 release가 안 와서 빔류가 계속 나간다
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAllAbilities();
	}

	// 이동 완전 정지 (행인 FreezeForMatchEnd와 동일 패턴 — 서버 정지가 이동 복제로 전 클라 정지)
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
}

TSubclassOf<UGameplayAbility> AMTPlayerCharacter::GetActivePassiveClass() const
{
	// PossessedBy의 고양이 결정과 동일: 선택값 우선, 없으면 폴백
	EMTCatType Cat = EMTCatType::None;
	if (const AMTPlayerState* PS = GetPlayerState<AMTPlayerState>())
	{
		Cat = PS->GetSelectedCat();
	}
	if (Cat == EMTCatType::None)
	{
		Cat = DefaultCatType;
	}

	if (const FMTCatAbilitySet* Set = CatAbilities.Find(Cat))
	{
		// 대표 패시브 = 첫 유효 항목 (고양이당 1개 전제, 여러 개면 첫 번째)
		for (const TSubclassOf<UGameplayAbility>& Passive : Set->Passives)
		{
			if (Passive)
			{
				return Passive;
			}
		}
	}
	return nullptr;
}

void AMTPlayerCharacter::ApplyLobbyDisguise()
{
	// 로비에서만: 남의 폰을 실루엣 머티리얼로 덮어 선택 고양이를 감춘다. 매치에선 원래 외형(무효).
	UWorld* World = GetWorld();
	if (!World || !World->GetGameState<AMTLobbyGameState>())
	{
		return;
	}
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}

	// 소유 판정은 BeginPlay(호스트=미possess)와 possession 후에 뒤바뀔 수 있으므로 양방향 보정.
	// 한 방향(덮기)만 하면 BeginPlay에서 자기 폰을 덮은 뒤 복원되지 않아 검게 굳는다.
	if (IsLocallyControlled() || !LobbyDisguiseMaterial)
	{
		// 내 폰: 원본 고양이로 복원 (덮였던 경우 되돌림)
		for (int32 i = 0; i < OriginalMaterials.Num(); ++i)
		{
			MeshComp->SetMaterial(i, OriginalMaterials[i]);
		}
		return;
	}

	// 남의 폰: 실루엣으로 덮기 (세 고양이가 메시를 공유 → 종류 은폐)
	const int32 NumMaterials = MeshComp->GetNumMaterials();
	for (int32 i = 0; i < NumMaterials; ++i)
	{
		MeshComp->SetMaterial(i, LobbyDisguiseMaterial);
	}
}

void AMTPlayerCharacter::ApplyLocalInputMode()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	// 로비·매치 공통: 조작만 → GameOnly + 커서 숨김. (로비 액션도 전부 키 입력)
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

void AMTPlayerCharacter::SetUICursorEnabled(bool bEnable)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	if (bEnable)
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);
	}
	else
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
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

void AMTPlayerCharacter::ToggleReady()
{
	// 로비에서만 — 로비 GameState가 있으면 로비 맵. 매치에선 무시.
	if (!GetWorld() || !GetWorld()->GetGameState<AMTLobbyGameState>())
	{
		return;
	}
	if (AMTPlayerController* PC = Cast<AMTPlayerController>(GetController()))
	{
		PC->Server_ToggleReady();   // 0.5s 쿨다운·검증은 컨트롤러가 처리
	}
}

void AMTPlayerCharacter::Interact()
{
	if (!AbilitySystemComponent || IsStunned())
	{
		return;
	}

	AbilitySystemComponent->TryActivateAbilitiesByTag(
		FGameplayTagContainer(MTGameplayTags::Ability::TAG_Skill_Interact), true);
}

void AMTPlayerCharacter::OnSkillAPressed()
{
	if (!AbilitySystemComponent || IsStunned())
	{
		return;
	}
	AbilitySystemComponent->AbilityLocalInputPressed((int32)EMTAbilitySlot::SkillA);
}

void AMTPlayerCharacter::OnSkillAReleased()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AbilityLocalInputReleased((int32)EMTAbilitySlot::SkillA);
	}
}

void AMTPlayerCharacter::OnSkillBPressed()
{
	if (!AbilitySystemComponent || IsStunned())
	{
		return;
	}
	AbilitySystemComponent->AbilityLocalInputPressed((int32)EMTAbilitySlot::SkillB);
}

void AMTPlayerCharacter::OnSkillBReleased()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AbilityLocalInputReleased((int32)EMTAbilitySlot::SkillB);
	}
}

void AMTPlayerCharacter::Die(AController* KillerController)
{
	if (!HasAuthority() || bIsDead)
	{
		return;
	}
	bIsDead = true;

	if (MTLogEnabled())
	{
		const FString VictimName = GetPlayerState() ? GetPlayerState()->GetPlayerName() : GetName();
		const FString KillerName = (KillerController && KillerController->PlayerState)
			? KillerController->PlayerState->GetPlayerName()
			: TEXT("Unknown");

		const FString Msg = FString::Printf(TEXT("[처치] %s → %s"), *KillerName, *VictimName);
		UE_LOG(LogMT, Warning, TEXT("%s"), *Msg);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(999, 8.f, FColor::Red, Msg);
		}
	}

	// 캐싱
	AController* MyController = GetController();

        // 이동/입력/충돌 정지 (애님 필요)
        if (UCharacterMovementComponent* Move = GetCharacterMovement())
        {
            Move->StopMovementImmediately();
            Move->DisableMovement();
        }
        SetActorEnableCollision(false);

        // 진행 중이던 스킬 캔슬
        if (AbilitySystemComponent)
        {
            AbilitySystemComponent->CancelAllAbilities();
        }

		// 사망 애니메이션 재생
		UAnimMontage* ChosenMontage = nullptr;
		float DeathLifeSpan = 2.0f; // 몽타주가 없을 때의 fallback 값

		if (DeathMontages.Num() > 0)
		{
			ChosenMontage = DeathMontages[FMath::RandRange(0, DeathMontages.Num() - 1)];
			DeathLifeSpan = ChosenMontage->GetPlayLength() + 1.5f;
		}
		MulticastPlayDeathMontage(ChosenMontage);

        if (MyController)
        {
            if (AMTMatchGameMode* GM = GetWorld()->GetAuthGameMode<AMTMatchGameMode>())
            {
                GM->RequestRespawn(MyController);
            }
        }

        // 컨트롤러와 분리만 하고 실제 파괴는 사망 이펙트 이후
        DetachFromControllerPendingDestroy();
        SetLifeSpan(DeathLifeSpan);
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

//착지 SFX 및 VFX
void AMTPlayerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (LandingSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, LandingSound, GetActorLocation());
	}

	if (LandingNiagaraSystem)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			LandingNiagaraSystem,
			GetMovementEffectLocation(),
			FRotator::ZeroRotator,
			FVector::OneVector,
			true,
			true,
			ENCPoolMethod::AutoRelease,
			true);
	}
}

//VFX 재생을 위한 점프 오버라이드
void AMTPlayerCharacter::Jump()
{
	if (IsStunned() || bIsDead || !CanJump())
	{
		return;
	}

	Super::Jump();

	if (JumpSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, JumpSound, GetActorLocation());
	}

	if (JumpNiagaraSystem)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			JumpNiagaraSystem,
			GetMovementEffectLocation(),
			FRotator::ZeroRotator,
			FVector::OneVector,
			true,
			true,
			ENCPoolMethod::AutoRelease,
			true);
	}
}

//걷기 이펙트 시작
UNiagaraComponent* AMTPlayerCharacter::SpawnWalkNiagara()
{
	if (IsValid(ActiveWalkNiagaraComponent))
	{
		return ActiveWalkNiagaraComponent;
	}

	if (!WalkNiagaraSystem)
	{
		return nullptr;
	}

	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const float CapsuleHalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.f;
	const FVector RelativeFootLocation = FVector(0.f, 0.f, -CapsuleHalfHeight) + MovementEffectOffset;

	ActiveWalkNiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
		WalkNiagaraSystem,
		GetRootComponent(),
		NAME_None,
		RelativeFootLocation,
		FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset,
		false,
		true,
		ENCPoolMethod::None,
		true);

	if (ActiveWalkNiagaraComponent)
	{
		ActiveWalkNiagaraComponent->SetVariableVec3(WalkDirectionParameterName, FVector::ZeroVector);
		ActiveWalkNiagaraComponent->SetVariableBool(WalkSpawnParameterName, false);
	}

	return ActiveWalkNiagaraComponent;
}

//걷기 이펙트 중단
void AMTPlayerCharacter::StopWalkNiagara()
{
	if (IsValid(ActiveWalkNiagaraComponent))
	{
		ActiveWalkNiagaraComponent->SetVariableBool(WalkSpawnParameterName, false);
	}
}

//걷기 이펙트 파라미터 업데이트
void AMTPlayerCharacter::UpdateMovementEffects()
{
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	const bool bShouldPlayWalkEffect = Movement
		&& !bIsDead
		&& !bStunned
		&& Movement->IsMovingOnGround()
		&& GetVelocity().SizeSquared2D() > FMath::Square(10.f);

	if (UNiagaraComponent* WalkFX = SpawnWalkNiagara())
	{
		const FVector MovementDirection = bShouldPlayWalkEffect
			? GetVelocity().GetSafeNormal2D()
			: FVector::ZeroVector;
		WalkFX->SetVariableVec3(WalkDirectionParameterName, MovementDirection);
		WalkFX->SetVariableBool(WalkSpawnParameterName, bShouldPlayWalkEffect);
	}
}

//걷기/점프 이펙트용 스폰 위치 계산. 캡슐 기준 하단부 확인해서 반환.
FVector AMTPlayerCharacter::GetMovementEffectLocation() const
{
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const float CapsuleHalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.f;
	return GetActorLocation() - FVector(0.f, 0.f, CapsuleHalfHeight) + MovementEffectOffset;
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

void AMTPlayerCharacter::MulticastPlayDeathMontage_Implementation(UAnimMontage* MontageToPlay)
{
	// bIsDead 복제 도착 전에 히트 큐가 올 수 있어, 모든 머신에서 로컬 확정 (큐 게이트용)
	bIsDead = true;

	UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	float PlayResult = -1.f;
	if (MontageToPlay && AnimInst)
	{
		AnimInst->StopAllMontages(0.f);   // 진행 중이던 히트/스킬 몽타주 정리 후 사망 재생
		PlayResult = AnimInst->Montage_Play(MontageToPlay);
	}

	if (MTLogEnabled())
	{
		// 진단: Montage=None → BP DeathMontages 미등록 / Result=0 → 스켈레톤 불일치 / Result>0인데 안 보임 → AnimBP Slot 누락
		const FString Msg = FString::Printf(TEXT("[MTDeath] Montage=%s AnimInst=%d PlayResult=%.2f | %s"),
			*GetNameSafe(MontageToPlay), AnimInst ? 1 : 0, PlayResult, *GetName());
		UE_LOG(LogMT, Log, TEXT("%s"), *Msg);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Magenta, Msg);
	}
}

bool AMTPlayerCharacter::IsEnemyCat(const AActor* SourceActor, const AActor* TargetActor)
{
	// 로비에선 폰-폰 데미지 금지 (조형물 선택만 허용). 매치에선 정상 판정.
	if (SourceActor && SourceActor->GetWorld() &&
		SourceActor->GetWorld()->GetGameState<AMTLobbyGameState>())
	{
		return false;
	}

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

void AMTPlayerCharacter::OnMoveSpeedMultChanged(const FOnAttributeChangeData& Data)
{
	// GE가 Full 복제로 전 머신에 도달 → 이동 권위(소유 클라)·서버 모두 속도 반영
	RecalcMoveSpeed();
}

void AMTPlayerCharacter::RecalcMoveSpeed()
{
	float Mult = 1.f;
	if (AbilitySystemComponent)
	{
		Mult = AbilitySystemComponent->GetNumericAttribute(UMTPlayerAttributeSet::GetMoveSpeedMultAttribute());
	}
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = BaseWalkSpeed * FMath::Max(Mult, 0.05f);
	}
}

void AMTPlayerCharacter::OnRep_IsDead()
{
	// 필요하면 여기서 클라이언트 측 후처리
}
