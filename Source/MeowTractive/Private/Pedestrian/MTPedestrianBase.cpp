// MTPedestrianBase.cpp

#include "Pedestrian/MTPedestrianBase.h"

#include "Pedestrian/MTAttractiveComponent.h"
#include "Pedestrian/MTPedestrianAttributeSet.h"
#include "Game/MTGameplayTags.h"
#include "UI/InGame/MTAttractivenessBarWidget.h"
#include "Player/MTPlayerState.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "Game/MTGameState.h"
#include "Net/UnrealNetwork.h"

// 자기 자신에게 GE 적용 (회복/태그/무적 — 전부 데이터 주도형)
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
	AttractiveBarWidget->SetDrawSize(FVector2D(200.f, 20.f));
}

void AMTPedestrianBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// 기여도 테이블 전체가 아닌 '결과(소유자)'만 복제
	DOREPLIFETIME(AMTPedestrianBase, AttractedBy);
	DOREPLIFETIME(AMTPedestrianBase, LeadingPlayer);
}

UAbilitySystemComponent* AMTPedestrianBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

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

void AMTPedestrianBase::BeginPlay()
{
	Super::BeginPlay();

	InvulnerableTag = MTGameplayTags::Pedestrian::Invulnerable.GetTag();
	AttractiveInProgressTag = MTGameplayTags::Pedestrian::AttractiveInProgress.GetTag();

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
		// 회복 GE 1회 부여: 무한+주기. AttractiveInProgress/Invulnerable 태그가 있으면 Ongoing Req로 정지.
		// 배회/이동은 StateTree(ST_Pedestrian)가 시작·관리한다.
		ApplySelfGE(AbilitySystemComponent, RegenGE, this);
	}

	BroadcastAttractiveChanged(); // 초기값 바 반영
}

void AMTPedestrianBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

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

void AMTPedestrianBase::HandleAttractiveHit(APlayerState* Source, float Amount)
{
	if (!HasAuthority() || !AttractiveComponent || !Source || Amount <= 0.f)
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

	// 7초 전투중 태그 갱신 → 회복 GE 정지 (AttractiveInProgressGE는 적용 시 Duration Refresh)
	ApplySelfGE(AbilitySystemComponent, AttractiveInProgressGE, this);

	BroadcastAttractiveChanged();

	if (NewAttractiveAmount >= AttractiveComponent->GetMaxAttractiveAmount() && !bIsAttracted)
	{
		BecomeAttracted(Source);
	}
}

void AMTPedestrianBase::HandleAttractiveRegen(float Amount)
{
	if (!HasAuthority() || !AttractiveComponent || Amount <= 0.f)
	{
		return;
	}

	AttractiveComponent->DecreaseAllAttractiveAmounts(Amount);
	UpdateLeadingPlayer();
	BroadcastAttractiveChanged();

	// 모든 플레이어 매료 수치가 0이면 중립 복귀한다.
	if (AttractiveComponent->GetHighestAttractiveAmount() <= 0.f
		&& (AttractedBy || LeadingPlayer || bIsAttracted))
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

void AMTPedestrianBase::UpdateLeadingPlayer()
{
	LeadingPlayer = AttractiveComponent ? AttractiveComponent->GetLeadingPlayer() : nullptr;
}

void AMTPedestrianBase::BecomeAttracted(APlayerState* Winner)
{
	if (!Winner)
	{
		return;
	}
	bIsAttracted = true;
	AttractedBy = Winner;

	// 즉시 이동 정지: 배회 MoveTo 중단 (StateTree가 Attracted로 전환되도록)
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
	}

	// 15초 무적 (State.Invulnerable) — 3초 응시와 별개. 회복도 이 동안 정지.
	ApplySelfGE(AbilitySystemComponent, InvulnerableGE, this);

	// 점수: 매료 소유자 +1
	if (AMTGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMTGameState>() : nullptr)
	{
		GS->AddAttractedCount(Winner);
	}

	BroadcastAttractiveChanged();
	// StateTree/BP가 구독 → Attracted 상태로 전환(이동 정지, 3초).
	OnAttracted.Broadcast(Winner);
}

void AMTPedestrianBase::EndAttracted()
{
	if (!HasAuthority())
	{
		return;
	}
	// 3초 응시만 종료(이동 재개). 무적(15초)·회복(GE)·소유자는 그대로 유지.
	bIsAttracted = false;
}

void AMTPedestrianBase::ResetAttractiveAmounts()
{
	if (AttractiveComponent)
	{
		AttractiveComponent->ResetAllAttractiveAmounts();
	}
	LeadingPlayer = nullptr;
}

void AMTPedestrianBase::OnRep_Attracted()
{
	BroadcastAttractiveChanged(); // 바 색 갱신 (회전은 서버 복제로 처리됨)
}

void AMTPedestrianBase::OnRep_Leader()
{
	BroadcastAttractiveChanged();
}

FLinearColor AMTPedestrianBase::GetLeaderColor() const
{
	const APlayerState* PS = AttractedBy ? AttractedBy : LeadingPlayer;
	if (const AMTPlayerState* MTPS = Cast<AMTPlayerState>(PS))
	{
		return MTPS->GetTeamColor();
	}
	return FLinearColor::White;
}

void AMTPedestrianBase::BroadcastAttractiveChanged()
{
	const float Amount = GetHighestAttractiveAmountValue();
	const float Max = GetMaxAttractiveAmountValue();
	OnAttractiveAmountChanged.Broadcast(Amount, Max);

	// 매료도 바 직접 갱신. 여러 플레이어 중 최고 수치를 표시한다.
	if (AttractiveBarWidget)
	{
		if (UMTAttractivenessBarWidget* Bar = Cast<UMTAttractivenessBarWidget>(AttractiveBarWidget->GetUserWidgetObject()))
		{
			Bar->UpdateBar(Amount, Max, GetLeaderColor());
		}
	}
}

void AMTPedestrianBase::HandleAttractiveAmountsChanged()
{
	if (HasAuthority())
	{
		UpdateLeadingPlayer();
	}
	BroadcastAttractiveChanged();
}

float AMTPedestrianBase::GetAttractiveAmountValue(APlayerState* TargetPlayerState) const
{
	return AttractiveComponent ? AttractiveComponent->GetAttractiveAmount(TargetPlayerState) : 0.f;
}

float AMTPedestrianBase::GetHighestAttractiveAmountValue() const
{
	return AttractiveComponent ? AttractiveComponent->GetHighestAttractiveAmount() : 0.f;
}

float AMTPedestrianBase::GetMaxAttractiveAmountValue() const
{
	return AttractiveComponent ? AttractiveComponent->GetMaxAttractiveAmount() : 100.f;
}

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
	AttractiveBarWidget->SetVisibility(FVector::Dist(GetActorLocation(), CamLoc) <= AttractiveBarVisibleDistance);
}
