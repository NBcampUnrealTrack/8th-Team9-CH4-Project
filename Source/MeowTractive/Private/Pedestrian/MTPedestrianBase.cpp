// MTPedestrianBase.cpp

#include "Pedestrian/MTPedestrianBase.h"

#include "Pedestrian/MTAttractiveComponent.h"
#include "Pedestrian/MTPedestrianAttributeSet.h"
#include "Pedestrian/MTPedestrianSpawnManager.h"
#include "Game/MTGameplayTags.h"
#include "UI/InGame/MTAttractivenessBarWidget.h"
#include "Player/MTPlayerState.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "Game/MTGameState.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

// 행인 자신의 ASC에 Regen 또는 전역 상태 GameplayEffect를 적용한다.
static void ApplySelfGE(UAbilitySystemComponent* ASC, TSubclassOf<UGameplayEffect> GE, AActor* Source)
{
	if (!ASC || !GE)
	{
		return;
	}
	FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
	Ctx.AddSourceObject(Source);
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(GE, 1.f, Ctx);
	if (Spec.IsValid())
	{
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

// 행인의 이동, GAS, 매료도 Component와 UI Component를 생성한다.
AMTPedestrianBase::AMTPedestrianBase()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(true);
	bUseControllerRotationYaw = false;

	// 스폰된 행인도 AIController가 붙어야 StateTree/MoveTo가 동작 (이거 없으면 안 움직임)
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = WalkSpeed;
		Movement->bOrientRotationToMovement = true;
		Movement->RotationRate = FRotator(0.0f, 360.0f, 0.0f);
	}

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	// AI(소유 클라 없음) → Minimal: GE는 복제 안 하고 속성/태그/큐만 복제
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	//매료도 수치 관련 컴포넌트
	AttributeSet = CreateDefaultSubobject<UMTPedestrianAttributeSet>(TEXT("AttractiveAttributeSet"));
	AttractiveComponent = CreateDefaultSubobject<UMTAttractiveComponent>(TEXT("AttractiveComponent"));

	AttractiveBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("AttractiveBar"));
	AttractiveBarWidget->SetupAttachment(GetMesh());
	AttractiveBarWidget->SetRelativeLocation(FVector(0.f, 0.f, 200.f));
	AttractiveBarWidget->SetWidgetSpace(EWidgetSpace::Screen);
	AttractiveBarWidget->SetDrawSize(FVector2D(240.f, 32.f));

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> AttractedFXAsset(
		TEXT("/Game/Niagara/NS_PedAttracted.NS_PedAttracted"));
	if (AttractedFXAsset.Succeeded())
	{
		AttractedFX = AttractedFXAsset.Object;
	}
}

// 행인의 매료 결과와 현재 선두 플레이어를 복제 속성으로 등록한다.
void AMTPedestrianBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// 기여도 테이블 전체가 아닌 '결과(소유자)'만 복제
	DOREPLIFETIME(AMTPedestrianBase, AttractedBy);
	DOREPLIFETIME(AMTPedestrianBase, LeadingPlayer);
	DOREPLIFETIME(AMTPedestrianBase, Appearance);
}

void AMTPedestrianBase::SetAppearance(const FMTPedestrianAppearance& NewAppearance)
{
	Appearance = NewAppearance;
	UMTPedestrianSpawnManager::ApplyAppearance(this, Appearance);
}

void AMTPedestrianBase::OnRep_Appearance()
{
	UMTPedestrianSpawnManager::ApplyAppearance(this, Appearance);
}

// 행인이 소유한 AbilitySystemComponent를 반환한다.
UAbilitySystemComponent* AMTPedestrianBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

// 매치 종료 시 서버에서 행인의 AI와 이동을 완전히 중지한다.
void AMTPedestrianBase::FreezeForMatchEnd()
{
	if (!HasAuthority())
	{
		return;
	}

	// StateTree(브레인) 로직 중단 + 진행 중 MoveTo 취소
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
		if (UBrainComponent* Brain = AI->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("MatchEnded"));
		}
	}

	// 이동 완전 정지 (재요청돼도 안 움직이게 MOVE_None)
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}

	SetActorTickEnabled(false);
}

void AMTPedestrianBase::Multicast_PlayAttractionTickSFX_Implementation()
{
	UGameplayStatics::PlaySoundAtLocation(this, AttractionTickSound, GetActorLocation());
}

void AMTPedestrianBase::Multicast_PlayAttractionCompleteSFX_Implementation()
{
	UGameplayStatics::PlaySoundAtLocation(this, AttractionCompleteSound, GetActorLocation());
}

// GAS ActorInfo와 Regen GE를 초기화하고 최초 UI 값을 반영한다.
void AMTPedestrianBase::BeginPlay()
{
	Super::BeginPlay();

	InvulnerableTag = MTGameplayTags::Pedestrian::Invulnerable.GetTag();

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = WalkSpeed;
	}

	if (HasAuthority())
	{
		// 회복 GE 1회 부여: 무한+주기. 플레이어별 감소 대기시간은 AttractiveComponent가 판정한다.
		// 배회/이동은 StateTree(ST_Pedestrian)가 시작·관리한다.
		ApplySelfGE(AbilitySystemComponent, RegenGE, this);
	}

	BroadcastAttractiveChanged(); // 초기값 바 반영
}

// AI Controller 빙의 후 GAS ActorInfo를 다시 연결한다.
void AMTPedestrianBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

// UI 가시성과 매료 완료 상태의 플레이어 방향 회전을 갱신한다.
void AMTPedestrianBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 바 가시성만 주기적으로 토글 (값 갱신은 델리게이트 — Tick 렌더링 아님)
	BarVisibilityTimer += DeltaTime;
	if (BarVisibilityTimer >= 0.1f)
	{
		BarVisibilityTimer = 0.f;
		UpdateAttractiveBarVisibility();
	}

	if (!HasAuthority())
	{
		return;
	}

	// 매료됨: 소유 플레이어를 향해 회전 (서버 회전 → 이동 복제로 클라 반영).
	// 이동 정지 자체는 StateTree의 Attracted 상태가 담당.
	if (bIsAttracted && AttractedBy)
	{
		if (const APawn* TargetPawn = AttractedBy->GetPawn())
		{
			FVector Dir = TargetPawn->GetActorLocation() - GetActorLocation();
			Dir.Z = 0.f;
			if (!Dir.IsNearlyZero())
			{
				const FRotator Target(0.f, Dir.Rotation().Yaw, 0.f);
				SetActorRotation(FMath::RInterpTo(GetActorRotation(), Target, DeltaTime, AttractiveTurnSpeed));
			}
		}
	}
}

// --- 매료 처리 (AttributeSet에서 서버 호출) ---

// 서버에서 공격한 플레이어의 매료도만 증가시키고 개인 Regen 대기시간을 갱신한다.
void AMTPedestrianBase::HandleAttractiveHit(APlayerState* Source, float Amount)
{
	if (!HasAuthority() || !AttractiveComponent || !Source || Amount <= 0.f)
	{
		return;
	}

	if (AttractedBy == Source)
	{
		return;
	}

	// 매료 직후 15초 무적 중엔 무시 (3초 응시와 별개)
	if (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(InvulnerableTag))
	{
		return;
	}

	const float NewAttractiveAmount = AttractiveComponent->AddAttractiveAmount(Source, Amount);
	UpdateLeadingPlayer();

	BroadcastAttractiveChanged();

	if (NewAttractiveAmount >= AttractiveComponent->GetMaxAttractiveAmount() && !bIsAttracted)
	{
		// 한 플레이어가 100에 도달하면 경쟁 플레이어 수치는 즉시 0으로 초기화한다.
		AttractiveComponent->ResetOtherAttractiveAmounts(Source);
		UpdateLeadingPlayer();
		BecomeAttracted(Source); // ← 완료 사운드는 이 안에서 재생
	}
	else
	{
		Multicast_PlayAttractionTickSFX(); // ← 매료도가 "올라간" 순간 (완료가 아닐 때만)
	}
}

// 서버에서 전역 무적 상태가 아닐 때 감소 가능한 플레이어 항목만 감소시킨다.
void AMTPedestrianBase::HandleAttractiveRegen(float Amount)
{
	if (!HasAuthority() || !AttractiveComponent || Amount <= 0.f)
	{
		return;
	}

	// 매료 완료 직후 전역 무적 동안에는 어떤 플레이어의 수치도 감소시키지 않는다.
	if (AbilitySystemComponent
		&& AbilitySystemComponent->HasMatchingGameplayTag(InvulnerableTag))
	{
		return;
	}

	AttractiveComponent->DecreaseEligibleAttractiveAmounts(Amount);
	UpdateLeadingPlayer();
	BroadcastAttractiveChanged();

	// 모든 플레이어 매료 수치가 0이면 중립 복귀한다.
	if (AttractiveComponent->GetHighestAttractiveAmount() <= 0.f && (AttractedBy || LeadingPlayer || bIsAttracted))
	{
		if (AttractedBy)
		{
			if (AMTGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMTGameState>() : nullptr)
			{
				GS->RemoveAttractedCount(AttractedBy);
			}
		}
		ResetAttractiveAmounts();
		AttractedBy = nullptr;
		bIsAttracted = false;
	}
}

// Component의 현재 수치를 기준으로 선두 플레이어를 갱신한다.
void AMTPedestrianBase::UpdateLeadingPlayer()
{
	LeadingPlayer = AttractiveComponent ? AttractiveComponent->GetLeadingPlayer() : nullptr;
}

// 100에 도달한 플레이어를 매료 소유자로 설정하고 전역 무적 상태를 적용한다.
void AMTPedestrianBase::BecomeAttracted(APlayerState* Winner)
{
	if (!Winner)
	{
		return;
	}

	APlayerState* PreviousOwner = AttractedBy;
	bIsAttracted = true;
	AttractedBy = Winner;

	// 즉시 이동 정지: 배회 MoveTo 중단 (StateTree가 Attracted로 전환되도록)
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
	}

	// 15초 무적 (State.Invulnerable) — 3초 응시와 별개. 회복도 이 동안 정지.
	ApplySelfGE(AbilitySystemComponent, InvulnerableGE, this);

	if (PreviousOwner != Winner)
	{
		FLinearColor PlayerColor = FLinearColor::White;
		if (const AMTPlayerState* MTPS = Cast<AMTPlayerState>(Winner))
		{
			PlayerColor = MTPS->GetTeamColor();
		}
		Multicast_PlayAttractedEffect(PlayerColor);
		Multicast_PlayAttractionCompleteSFX();
	}

	// 점수: 소유자가 바뀌면 기존 소유자 -1, 새 소유자 +1
	if (AMTGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMTGameState>() : nullptr)
	{
		if (PreviousOwner != Winner)
		{
			if (PreviousOwner)
			{
				GS->RemoveAttractedCount(PreviousOwner);
			}
			GS->AddAttractedCount(Winner);
		}
	}

	BroadcastAttractiveChanged();
	// StateTree/BP가 구독 → Attracted 상태로 전환(이동 정지, 3초).
	OnAttracted.Broadcast(Winner);
}

void AMTPedestrianBase::Multicast_PlayAttractedEffect_Implementation(FLinearColor PlayerColor)
{
	if (!AttractedFX)
	{
		return;
	}

	UNiagaraComponent* AttractedFXComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
		AttractedFX,
		GetRootComponent(),
		NAME_None,
		AttractedFXOffset,
		FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset,
		true);

	if (AttractedFXComponent)
	{
		AttractedFXComponent->SetVariableLinearColor(FName(TEXT("User.PlayerColor")), PlayerColor);
	}
}

// StateTree의 매료 반응 시간이 끝나면 행인의 회전·정지 상태를 해제한다.
void AMTPedestrianBase::EndAttracted()
{
	if (!HasAuthority())
	{
		return;
	}
	// 3초 응시만 종료(이동 재개). 무적(15초)·회복(GE)·소유자는 그대로 유지.
	bIsAttracted = false;
}

// 모든 플레이어 매료도와 선두 플레이어를 초기화한다.
void AMTPedestrianBase::ResetAttractiveAmounts()
{
	if (AttractiveComponent)
	{
		AttractiveComponent->ResetAllAttractiveAmounts();
	}
	LeadingPlayer = nullptr;
}

// 클라이언트에서 매료 소유자 복제 변경을 UI에 반영한다.
void AMTPedestrianBase::OnRep_Attracted()
{
	BroadcastAttractiveChanged(); // 바 색 갱신 (회전은 서버 복제로 처리됨)
}

// 클라이언트에서 선두 플레이어 복제 변경을 UI에 반영한다.
void AMTPedestrianBase::OnRep_Leader()
{
	BroadcastAttractiveChanged();
}

// 매료 소유자 또는 현재 선두 플레이어의 팀 색상을 반환한다.
FLinearColor AMTPedestrianBase::GetLeaderColor() const
{
	const APlayerState* PS = AttractedBy ? AttractedBy : LeadingPlayer;
	if (const AMTPlayerState* MTPS = Cast<AMTPlayerState>(PS))
	{
		return MTPS->GetTeamColor();
	}
	return FLinearColor::White;
}

// 현재 로컬 플레이어와 경쟁자 매료도 값을 World Space UI에 전달한다.
void AMTPedestrianBase::BroadcastAttractiveChanged()
{
	const float Amount = GetHighestAttractiveAmountValue();
	const float Max = GetMaxAttractiveAmountValue();
	OnAttractiveAmountChanged.Broadcast(Amount, Max);

	// 로컬 플레이어 수치는 전경, 다른 플레이어 최고 수치는 배경에 겹쳐 표시한다.
	if (AttractiveBarWidget)
	{
		if (UMTAttractivenessBarWidget* Bar = Cast<UMTAttractivenessBarWidget>(AttractiveBarWidget->GetUserWidgetObject()))
		{
			const APlayerController* LocalPC =
				GetWorld() ? GEngine->GetFirstLocalPlayerController(GetWorld()) : nullptr;
			APlayerState* LocalPlayerState = LocalPC ? LocalPC->PlayerState : nullptr;

			const float CurrentAmount = AttractiveComponent
				? AttractiveComponent->GetAttractiveAmount(LocalPlayerState)
				: 0.f;
			const float EnemyAmount = AttractiveComponent
				? AttractiveComponent->GetHighestAttractiveAmountExcluding(LocalPlayerState)
				: 0.f;
			const APlayerState* EnemyPlayerState = AttractiveComponent
				? AttractiveComponent->GetLeadingPlayerExcluding(LocalPlayerState)
				: nullptr;

			FLinearColor CurrentColor = FLinearColor(1.f, 0.15f, 0.4f, 1.f);
			if (const AMTPlayerState* CurrentMTPS = Cast<AMTPlayerState>(LocalPlayerState))
			{
				CurrentColor = CurrentMTPS->GetTeamColor();
			}

			FLinearColor EnemyColor = FLinearColor(0.45f, 0.2f, 1.f, 1.f);
			if (const AMTPlayerState* EnemyMTPS = Cast<AMTPlayerState>(EnemyPlayerState))
			{
				EnemyColor = EnemyMTPS->GetTeamColor();
			}

			Bar->UpdateBars(CurrentAmount, EnemyAmount, Max, CurrentColor, EnemyColor);
		}
	}
}

// 서버 또는 FastArray 수신 후 선두 상태와 UI를 갱신한다.
void AMTPedestrianBase::HandleAttractiveAmountsChanged()
{
	if (HasAuthority())
	{
		UpdateLeadingPlayer();
	}
	BroadcastAttractiveChanged();
}

// 지정한 플레이어의 매료도를 Component에서 조회한다.
float AMTPedestrianBase::GetAttractiveAmountValue(APlayerState* TargetPlayerState) const
{
	return AttractiveComponent ? AttractiveComponent->GetAttractiveAmount(TargetPlayerState) : 0.f;
}

// 모든 플레이어 중 최고 매료도를 Component에서 조회한다.
float AMTPedestrianBase::GetHighestAttractiveAmountValue() const
{
	return AttractiveComponent ? AttractiveComponent->GetHighestAttractiveAmount() : 0.f;
}

// Component에 설정된 최대 매료도를 반환한다.
float AMTPedestrianBase::GetMaxAttractiveAmountValue() const
{
	return AttractiveComponent ? AttractiveComponent->GetMaxAttractiveAmount() : 100.f;
}

// 로컬 카메라와 거리를 기준으로 매료도 UI 가시성을 결정한다.
void AMTPedestrianBase::UpdateAttractiveBarVisibility()
{
	if (!AttractiveBarWidget)
	{
		return;
	}
	const APlayerController* PC = GetWorld() ? GEngine->GetFirstLocalPlayerController(GetWorld()) : nullptr;
	if (!PC)
	{
		return;
	}

	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	const float DistSq = FVector::DistSquared(GetActorLocation(), CamLoc);
	if (DistSq > FMath::Square(AttractiveBarVisibleDistance))
	{
		AttractiveBarWidget->SetVisibility(false);
		return;
	}

	// 거리 안이면 벽에 가려졌는지 라인트레이스로 확인
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	if (const APawn* LocalPawn = PC->GetPawn())
	{
		Params.AddIgnoredActor(LocalPawn);
	}

	const FVector TargetLoc = AttractiveBarWidget->GetComponentLocation();

	FHitResult Hit;
	const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
		Hit, CamLoc, TargetLoc, ECC_Visibility, Params);

	AttractiveBarWidget->SetVisibility(!bBlocked);
}
