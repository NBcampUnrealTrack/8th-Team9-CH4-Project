#include "Player/MTLobbyCharacter.h"

#include "Player/MTPlayerController.h"
#include "Player/MTPlayerState.h"
#include "Game/MTLobbyGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

AMTLobbyCharacter::AMTLobbyCharacter()
{
	// "C키로 선택" 프롬프트 — 화면공간 빌보드. WidgetClass는 BP에서 지정. (조형물 숨김 시 함께 숨음)
	PromptWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("PromptWidget"));
	PromptWidget->SetupAttachment(RootComponent);
	PromptWidget->SetWidgetSpace(EWidgetSpace::Screen);
	PromptWidget->SetDrawAtDesiredSize(true);
	PromptWidget->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
}

void AMTLobbyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMTLobbyCharacter, OwnerSlot);
}

void AMTLobbyCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 정적 조형물: 이동·중력 시뮬 정지 — 클라에서 충돌 꺼진 사이 중력으로 바닥 아래로 가라앉는 것 방지
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->DisableMovement();
		Move->SetComponentTickEnabled(false);
	}

	UpdateLobbyVisibility();
	// 로컬 슬롯·OwnerSlot 복제가 늦을 수 있어 주기적으로 재평가 (로비 HUD 폴링과 동일한 방식)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			VisibilityTimer, this, &AMTLobbyCharacter::UpdateLobbyVisibility, 0.4f, true);
	}
}

void AMTLobbyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(VisibilityTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void AMTLobbyCharacter::UpdateLobbyVisibility()
{
	// 로컬 플레이어 슬롯과 OwnerSlot 비교 — 내 슬롯 조형물만 표시
	int32 LocalSlot = -1;
	if (const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (const AMTPlayerState* PS = PC->GetPlayerState<AMTPlayerState>())
		{
			LocalSlot = PS->GetPlayerSlot();
		}
	}

	const bool bMine = (OwnerSlot >= 0 && OwnerSlot == LocalSlot);
	SetActorHiddenInGame(!bMine);
	// 서버는 펀치 판정 위해 충돌 유지. 순수 클라만 남 조형물 충돌 해제.
	if (!HasAuthority())
	{
		SetActorEnableCollision(bMine);
	}
}

void AMTLobbyCharacter::OnPunchSelect(AActor* InstigatorPawn)
{
	// 서버 권위 + 로비 GameMode에서만 동작
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !World->GetAuthGameMode<AMTLobbyGameMode>())
	{
		return;
	}

	// 소유자 검증: 이 조형물의 슬롯 플레이어만 조작 가능
	const APawn* Pawn = Cast<APawn>(InstigatorPawn);
	AMTPlayerController* PC = Pawn ? Cast<AMTPlayerController>(Pawn->GetController()) : nullptr;
	const AMTPlayerState* PS = PC ? PC->GetPlayerState<AMTPlayerState>() : nullptr;
	if (!PS || PS->GetPlayerSlot() != OwnerSlot)
	{
		return;
	}

	// 다음 조형물 스폰 (같은 위치, 슬롯 승계). 실패 시 자기 유지.
	if (!SpawnNextCat())
	{
		return;
	}

	// 때린 순간 보이던 고양이(이 조형물)를 선택 (준비 검증·로비 폰 재스폰은 컨트롤러가 처리)
	PC->Server_SetSelectedCat(RepresentedCat);

	Destroy();
}

AMTLobbyCharacter* AMTLobbyCharacter::SpawnNextCat()
{
	UWorld* World = GetWorld();
	if (!World || !NextCatActorClass)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AMTLobbyCharacter* NextActor = World->SpawnActor<AMTLobbyCharacter>(
		NextCatActorClass, GetActorTransform(), Params);
	if (NextActor)
	{
		NextActor->OwnerSlot = OwnerSlot;
	}
	return NextActor;
}

int32 AMTLobbyCharacter::GetCycleLength() const
{
	// 체인이 자기 클래스로 돌아오면 한 바퀴. 끊겨 있으면 거기까지가 길이.
	const UClass* const StartClass = GetClass();
	int32 Length = 1;
	TSubclassOf<AMTLobbyCharacter> Next = NextCatActorClass;

	while (Next && Next.Get() != StartClass && Length < 16)   // 16 = 체인 오설정 시 무한루프 방지
	{
		++Length;
		Next = Next.GetDefaultObject()->NextCatActorClass;
	}
	return Length;
}

AMTLobbyCharacter* AMTLobbyCharacter::AdvanceCycle(int32 Steps)
{
	if (!HasAuthority())
	{
		return this;
	}

	AMTLobbyCharacter* Current = this;
	for (int32 i = 0; i < Steps; ++i)
	{
		AMTLobbyCharacter* Next = Current->SpawnNextCat();
		if (!Next)
		{
			break;   // 체인 오설정 — 현재 조형물 유지
		}
		Current->Destroy();
		Current = Next;
	}
	return Current;
}
