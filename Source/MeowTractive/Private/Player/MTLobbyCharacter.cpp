#include "Player/MTLobbyCharacter.h"

#include "Player/MTPlayerController.h"
#include "Player/MTPlayerState.h"
#include "Game/MTLobbyGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "UI/Lobby/MTSelectPromptWidget.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AMTLobbyCharacter::AMTLobbyCharacter()
{
	// "C키로 선택" 프롬프트 — 화면공간 빌보드. WidgetClass는 BP에서 지정. (조형물 숨김 시 함께 숨음)
	PromptWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("PromptWidget"));
	PromptWidget->SetupAttachment(RootComponent);
	PromptWidget->SetWidgetSpace(EWidgetSpace::Screen);
	PromptWidget->SetDrawAtDesiredSize(true);
	PromptWidget->SetRelativeLocation(FVector(0.f, 0.f, 120.f));

	// 기본 사이클 (플레이어 폰 BP와 동일한 머티리얼 — 점박이→고등어→뚱냥이)
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SpottedMat(TEXT("/Game/Cat_Simple/Cat/Toon/M_CatSimple_C7_Toon"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MackerelMat(TEXT("/Game/Cat_Simple/Cat/Toon/M_CatSimple_C6_Toon"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FattyMat(TEXT("/Game/Cat_Simple/Cat/Toon/M_CatSimple_C5_Toon"));
	if (SpottedMat.Succeeded() && MackerelMat.Succeeded() && FattyMat.Succeeded())
	{
		CatOptions = {
			{ EMTCatType::Spotted,  SpottedMat.Object },
			{ EMTCatType::Mackerel, MackerelMat.Object },
			{ EMTCatType::Fatty,    FattyMat.Object },
		};
	}
}

void AMTLobbyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMTLobbyCharacter, OwnerSlot);
	DOREPLIFETIME(AMTLobbyCharacter, CatIndex);
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

	// 서버: 초기 표시 = BP가 지정한 RepresentedCat (이후 셔플이 무작위로 덮음)
	if (HasAuthority() && CatIndex == INDEX_NONE)
	{
		CatIndex = 0;
		for (int32 i = 0; i < CatOptions.Num(); ++i)
		{
			if (CatOptions[i].Cat == RepresentedCat)
			{
				CatIndex = i;
				break;
			}
		}
	}
	ApplyCatMaterial();

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

void AMTLobbyCharacter::PushPromptLabel()
{
	if (UMTSelectPromptWidget* Prompt = PromptWidget
		? Cast<UMTSelectPromptWidget>(PromptWidget->GetUserWidgetObject()) : nullptr)
	{
		Prompt->SetActionLabel(PromptLabel);
	}
}

void AMTLobbyCharacter::UpdateLobbyVisibility()
{
	PushPromptLabel();   // 위젯 컴포넌트의 위젯 생성이 늦을 수 있어 주기 재시도 (멱등)

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

EMTCatType AMTLobbyCharacter::GetRepresentedCat() const
{
	return CatOptions.IsValidIndex(CatIndex) ? CatOptions[CatIndex].Cat : RepresentedCat;
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
	if (!PS || PS->GetPlayerSlot() != OwnerSlot || CatOptions.Num() == 0)
	{
		return;
	}

	// 때린 순간 보이던 고양이를 선택 (준비 검증·로비 폰 재스폰은 컨트롤러가 처리)
	PC->Server_SetSelectedCat(GetRepresentedCat());

	// 다음 후보로 머티리얼만 순환 — 액터 교체 없음
	CatIndex = (FMath::Max(0, CatIndex) + 1) % CatOptions.Num();
	OnRep_CatIndex();   // 리슨 호스트 즉시 반영
}

void AMTLobbyCharacter::OnRep_CatIndex()
{
	ApplyCatMaterial();
}

void AMTLobbyCharacter::ApplyCatMaterial()
{
	if (!CatOptions.IsValidIndex(CatIndex) || !CatOptions[CatIndex].Material)
	{
		return;
	}
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		const int32 NumMaterials = MeshComp->GetNumMaterials();
		for (int32 i = 0; i < NumMaterials; ++i)
		{
			MeshComp->SetMaterial(i, CatOptions[CatIndex].Material);
		}
	}
}
