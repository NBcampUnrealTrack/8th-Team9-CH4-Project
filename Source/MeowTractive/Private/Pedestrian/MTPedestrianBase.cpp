// MTPedestrianBase.cpp

#include "Pedestrian/MTPedestrianBase.h"

#include "Pedestrian/MTPedestrianAttributeSet.h"
#include "Game/MTGameplayTags.h"
#include "UI/InGame/MTAttractivenessBarWidget.h"
#include "Player/MTPlayerState.h"
#include "AIController.h"
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

	AttributeSet = CreateDefaultSubobject<UMTPedestrianAttributeSet>(TEXT("AttractiveAttributeSet"));

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

void AMTPedestrianBase::HandleAttractiveHit(APlayerState* Source, float Amount, float NewAttractiveHealth)
{
	if (!HasAuthority())
	{
		return;
	}
	// 매료 직후 15초 무적 중엔 무시 (3초 응시와 별개)
	if (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(InvulnerableTag))
	{
		return;
	}

	if (Source)
	{
		AttractiveContributions.FindOrAdd(Source) += Amount;
		LastAttacker = Source;
		LeadingPlayer = Source;   // 바 색용 (복제)
	}

	// 7초 전투중 태그 갱신 → 회복 GE 정지 (AttractiveInProgressGE는 적용 시 Duration Refresh)
	ApplySelfGE(AbilitySystemComponent, AttractiveInProgressGE, this);

	BroadcastAttractiveChanged();

	if (NewAttractiveHealth <= 0.f && !bIsAttracted)
	{
		BecomeAttracted(DetermineAttractiveWinner());
	}
}

void AMTPedestrianBase::HandleAttractiveRegen(float NewAttractiveHealth)
{
	if (!HasAuthority())
	{
		return;
	}
	BroadcastAttractiveChanged();

	// 체력 100 완전 회복 = 중립 복귀: 소유자 점수 회수 + 기여도 초기화
	if (NewAttractiveHealth >= GetMaxAttractiveHealthValue())
	{
		if (AttractedBy)
		{
			if (AMTGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMTGameState>() : nullptr)
			{
				GS->RemoveAttractedCount(AttractedBy);
			}
		}
		ResetContributions();   // 기여도 + LeadingPlayer 초기화
		AttractedBy = nullptr;
		bIsAttracted = false;
	}
}

APlayerState* AMTPedestrianBase::DetermineAttractiveWinner() const
{
	float MaxVal = -1.f;
	for (const TPair<TWeakObjectPtr<APlayerState>, float>& Pair : AttractiveContributions)
	{
		if (Pair.Key.IsValid() && Pair.Value > MaxVal)
		{
			MaxVal = Pair.Value;
		}
	}
	if (MaxVal <= 0.f)
	{
		return LastAttacker.Get(); // 폴백
	}

	// 최다 기여자 수집
	TArray<APlayerState*> Top;
	for (const TPair<TWeakObjectPtr<APlayerState>, float>& Pair : AttractiveContributions)
	{
		if (Pair.Key.IsValid() && FMath::IsNearlyEqual(Pair.Value, MaxVal, 0.01f))
		{
			Top.Add(Pair.Key.Get());
		}
	}

	if (Top.Num() == 1)
	{
		return Top[0];
	}
	// 동점 → 라스트힛 우선
	if (LastAttacker.IsValid() && Top.Contains(LastAttacker.Get()))
	{
		return LastAttacker.Get();
	}
	return Top.Num() > 0 ? Top[0] : nullptr;
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

void AMTPedestrianBase::ResetContributions()
{
	AttractiveContributions.Empty();
	LastAttacker = nullptr;
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
	const float Cur = GetAttractiveHealthValue();
	const float Max = GetMaxAttractiveHealthValue();
	OnAttractiveHealthChanged.Broadcast(Cur, Max);

	// 매료도 바 직접 갱신 (이벤트 기반 — Tick 렌더링 아님). 표시값 = Max-Current(쌓인 매료도)
	if (AttractiveBarWidget)
	{
		if (UMTAttractivenessBarWidget* Bar = Cast<UMTAttractivenessBarWidget>(AttractiveBarWidget->GetUserWidgetObject()))
		{
			Bar->UpdateBar(Max - Cur, Max, GetLeaderColor());
		}
	}
}

float AMTPedestrianBase::GetAttractiveHealthValue() const
{
	return AttributeSet ? AttributeSet->GetAttractiveHealth() : 0.f;
}

float AMTPedestrianBase::GetMaxAttractiveHealthValue() const
{
	return AttributeSet ? AttributeSet->GetMaxAttractiveHealth() : 0.f;
}

float AMTPedestrianBase::GetAttractiveGauge() const
{
	return GetMaxAttractiveHealthValue() - GetAttractiveHealthValue();
}

void AMTPedestrianBase::UpdateAttractiveBarVisibility()
{
	if (!AttractiveBarWidget)
	{
		return;
	}
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}
	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	AttractiveBarWidget->SetVisibility(FVector::Dist(GetActorLocation(), CamLoc) <= AttractiveBarVisibleDistance);
}
