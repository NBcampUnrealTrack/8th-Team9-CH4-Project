// MTPedestrianBase.cpp

#include "Pedestrian/MTPedestrianBase.h"

#include "AIController.h"
#include "NavigationSystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "UI/InGame/MTAttractivenessBarWidget.h"
#include "Net/UnrealNetwork.h"
#include "Player/MTPlayerState.h"
#include "TimerManager.h"

AMTPedestrianBase::AMTPedestrianBase()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(true);

	bUseControllerRotationYaw = false;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = WalkSpeed;
		Movement->bOrientRotationToMovement = true;
		Movement->RotationRate = FRotator(0.0f, 360.0f, 0.0f);
	}

	AttractivenessBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("AttractivenessBar"));
	AttractivenessBarWidget->SetupAttachment(GetMesh(), TEXT("head")); // 머리 소켓
	AttractivenessBarWidget->SetWidgetSpace(EWidgetSpace::World);
	AttractivenessBarWidget->SetDrawSize(FVector2D(200.f, 20.f));
	AttractivenessBarWidget->SetVisibility(false); // 기본 숨김
}

void AMTPedestrianBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMTPedestrianBase, Attractivenesses);
}

float AMTPedestrianBase::GetAttractiveness(APlayerState* TargetPlayerState) const
{
	if (!TargetPlayerState)
	{
		return 0.0f;
	}

	const FPedestrianAttractiveness* Entry = Attractivenesses.FindByPredicate(
		[TargetPlayerState](const FPedestrianAttractiveness& Item)
		{
			return Item.PlayerState == TargetPlayerState;
		}
	);

	return Entry ? Entry->Value : 0.0f;
}

void AMTPedestrianBase::AddAttractiveness(APlayerState* TargetPlayerState, float Delta)
{
	if (!TargetPlayerState || FMath::IsNearlyZero(Delta))
	{
		return;
	}

	if (!HasAuthority())
	{
		ServerAddAttractiveness(TargetPlayerState, Delta);
		return;
	}

	FPedestrianAttractiveness* Entry = Attractivenesses.FindByPredicate(
		[TargetPlayerState](const FPedestrianAttractiveness& Item)
		{
			return Item.PlayerState == TargetPlayerState;
		}
	);

	if (Entry)
	{
		Entry->Value = FMath::Max(0.0f, Entry->Value + Delta);
	}
	else
	{
		FPedestrianAttractiveness NewEntry;
		NewEntry.PlayerState = TargetPlayerState;
		NewEntry.Value = FMath::Max(0.0f, Delta);
		Attractivenesses.Add(NewEntry);
	}

	// 필요하면 여기서 팬 전환/최고 매료도 플레이어 판정
}

void AMTPedestrianBase::ServerAddAttractiveness_Implementation(APlayerState* TargetPlayerState, float Delta)
{
	AddAttractiveness(TargetPlayerState, Delta);
}

void AMTPedestrianBase::OnRep_Attractivenesses()
{
	// 클라이언트 UI/VFX 갱신이 필요하면 여기서 처리

	// 가장 높은 매료도 플레이어 찾기
    const FPedestrianAttractiveness* Top = nullptr;
    for (const FPedestrianAttractiveness& Entry : Attractivenesses)
    {
        if (!Top || Entry.Value > Top->Value)
            Top = &Entry;
    }

    if (!Top || Top->Value <= 0.f)
    {
        AttractivenessBarWidget->SetVisibility(false);
        return;
    }

    // 팀 색 가져오기 (PlayerState에서)
	AMTPlayerState* MTPS = Cast<AMTPlayerState>(Top->PlayerState);
	if (!MTPS) return;

	if (auto* Bar = Cast<UMTAttractivenessBarWidget>(AttractivenessBarWidget->GetUserWidgetObject()))
	{
		Bar->UpdateBar(Top->Value, MaxAttraction, MTPS->GetTeamColor());
		AttractivenessBarWidget->SetVisibility(true);
	}
}

//시작과 함께 돌아다니기 시작
void AMTPedestrianBase::BeginPlay()
{
	Super::BeginPlay();

	//이동속도 설정
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = WalkSpeed;
	}

	if (HasAuthority())
	{
		ResumeWander();
	}
}

void AMTPedestrianBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HasAuthority())
	{
		return;
	}

	if (!bIsWaiting && bHasDestination && HasReachedDestination())
	{
		WaitBeforeNextMove();
	}
}

//무작위 지점 선택 후 이동 시작
void AMTPedestrianBase::MoveToRandomLocation()
{
	if (bIsWaiting)
	{
		return;
	}

	AAIController* AIController = Cast<AAIController>(GetController());
	if (!AIController)
	{
		return;
	}

	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSystem)
	{
		return;
	}

	FNavLocation RandomLocation;
	const bool bFoundLocation = NavSystem->GetRandomReachablePointInRadius(
		GetActorLocation(),
		WanderRadius,
		RandomLocation
	);

	if (!bFoundLocation)
	{
		WaitBeforeNextMove();
		return;
	}

	CurrentDestination = RandomLocation.Location;
	bHasDestination = true;

	AIController->MoveToLocation(
		CurrentDestination,
		AcceptanceRadius,
		true,
		true,
		true,
		false
	);
}

//대기모드로 전환
void AMTPedestrianBase::WaitBeforeNextMove()
{
	bIsWaiting = true;
	bHasDestination = false;

	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->StopMovement();
	}

	const float WaitTime = FMath::RandRange(MinWaitTime, MaxWaitTime);

	GetWorldTimerManager().SetTimer(
		WaitTimerHandle,
		this,
		&AMTPedestrianBase::ResumeWander,
		WaitTime,
		false
	);
}

//걷기 모드로 전환
void AMTPedestrianBase::ResumeWander()
{
	bIsWaiting = false;
	MoveToRandomLocation();
}

//목표지점 도착 검사
bool AMTPedestrianBase::HasReachedDestination() const
{
	const float DistanceSquared = FVector::DistSquared2D(GetActorLocation(), CurrentDestination);
	return DistanceSquared <= FMath::Square(MoveCheckDistance);
}
