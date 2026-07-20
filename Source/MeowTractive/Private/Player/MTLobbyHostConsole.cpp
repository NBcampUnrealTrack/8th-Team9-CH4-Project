#include "Player/MTLobbyHostConsole.h"
#include "Player/MTPlayerCharacter.h"
#include "Player/MTPlayerState.h"
#include "Game/MTLobbyGameMode.h"
#include "Blueprint/UserWidget.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "UI/Lobby/MTSelectPromptWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "TimerManager.h"

AMTLobbyHostConsole::AMTLobbyHostConsole()
{
	PrimaryActorTick.bCanEverTick = false;

	// 펀치 오버랩(OverlapMultiByObjectType(ECC_Pawn))에 잡히도록 Pawn 타입 쿼리 박스
	InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
	SetRootComponent(InteractionBox);
	InteractionBox->SetBoxExtent(FVector(60.f, 60.f, 90.f));
	InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBox->SetCollisionObjectType(ECC_Pawn);
	InteractionBox->SetCollisionResponseToAllChannels(ECR_Overlap);

	PromptWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("PromptWidget"));
	PromptWidget->SetupAttachment(RootComponent);
	PromptWidget->SetWidgetSpace(EWidgetSpace::Screen);
	PromptWidget->SetDrawAtDesiredSize(true);
	PromptWidget->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
}

void AMTLobbyHostConsole::BeginPlay()
{
	Super::BeginPlay();

	InteractionBox->OnComponentBeginOverlap.AddDynamic(this, &AMTLobbyHostConsole::OnInteractionBoxBeginOverlap);
	InteractionBox->OnComponentEndOverlap.AddDynamic(this, &AMTLobbyHostConsole::OnInteractionBoxEndOverlap);

	UpdateHostVisibility();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			VisibilityTimer, this, &AMTLobbyHostConsole::UpdateHostVisibility, 0.4f, true);
	}
}

void AMTLobbyHostConsole::OnInteractionBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 로컬 플레이어 폰에게만 표시 (시각 연출 — 원격 폰 무시)
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (Pawn && Pawn->IsPlayerControlled() && Pawn->IsLocallyControlled())
	{
		MTLobbyOutline::SetActorOutline(this, true, OutlineStencil);
	}
}

void AMTLobbyHostConsole::OnInteractionBoxEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (Pawn && Pawn->IsPlayerControlled() && Pawn->IsLocallyControlled())
	{
		MTLobbyOutline::SetActorOutline(this, false, 0);
	}
}

void AMTLobbyHostConsole::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(VisibilityTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void AMTLobbyHostConsole::PushPromptLabel()
{
	if (UMTSelectPromptWidget* Prompt = PromptWidget
		? Cast<UMTSelectPromptWidget>(PromptWidget->GetUserWidgetObject()) : nullptr)
	{
		Prompt->SetActionLabel(PromptLabel);
	}
}

void AMTLobbyHostConsole::UpdateHostVisibility()
{
	PushPromptLabel();   // 위젯 생성 지연 대비 주기 재시도 (멱등)

	bool bLocalIsHost = false;
	if (const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (const AMTPlayerState* PS = PC->GetPlayerState<AMTPlayerState>())
		{
			bLocalIsHost = PS->IsHost();
		}
	}

	SetActorHiddenInGame(!bLocalIsHost);
	// 서버(=호스트 머신)는 펀치 판정 위해 충돌 유지. 순수 클라는 오탐 방지로 해제.
	if (!HasAuthority())
	{
		SetActorEnableCollision(bLocalIsHost);
	}
}

void AMTLobbyHostConsole::OnPunchSelect(AActor* InstigatorPawn)
{
	// 서버 권위 + 로비 GameMode에서만
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !World->GetAuthGameMode<AMTLobbyGameMode>())
	{
		return;
	}

	// 호스트만 조작 가능 (리슨 서버 = 호스트 로컬이므로 위젯은 곧장 로컬 생성)
	const APawn* Pawn = Cast<APawn>(InstigatorPawn);
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	const AMTPlayerState* PS = PC ? PC->GetPlayerState<AMTPlayerState>() : nullptr;
	if (!PS || !PS->IsHost() || !PC->IsLocalPlayerController())
	{
		return;
	}

	if (!ConsoleWidgetClass)
	{
		return;
	}
	UUserWidget* Console = CreateWidget<UUserWidget>(PC, ConsoleWidgetClass);
	if (!Console)
	{
		return;
	}

	// UIOnly 전환 전에 시전 중 스킬 취소 (합성 release로 홀드 스킬 발사 방지)
	if (AMTPlayerCharacter* Cat = Cast<AMTPlayerCharacter>(PC->GetPawn()))
	{
		Cat->CancelActiveSkills();
	}

	Console->AddToViewport(40);

	// 일시정지 메뉴와 동일하게 UIOnly — 게임 입력 차단, 마우스/UI 입력만 (복원은 콘솔 위젯이 처리)
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Console->TakeWidget());
	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(true);
}
