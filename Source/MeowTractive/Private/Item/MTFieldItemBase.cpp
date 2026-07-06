// Fill out your copyright notice in the Description page of Project Settings.


#include "Item/MTFieldItemBase.h"
#include "Item/MTItemData.h"
#include "Game/MTGameState.h"
#include "Item/MTItemInventoryComponent.h"
#include "Components/SphereComponent.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"

// Sets default values
AMTFieldItemBase::AMTFieldItemBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->SetSphereRadius(50.f);
	RootComponent = CollisionSphere;

	ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
	ItemMesh->SetupAttachment(RootComponent);
	ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// Called when the game starts or when spawned
void AMTFieldItemBase::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &AMTFieldItemBase::OnOverlapBegin);
		ActivateWithRandomItem();
	}

	ApplyVisualState();

}

void AMTFieldItemBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		GetWorldTimerManager().ClearTimer(RespawnTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void AMTFieldItemBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMTFieldItemBase, CurrentItemType);
	DOREPLIFETIME(AMTFieldItemBase, bIsAvailable);
}

void AMTFieldItemBase::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || !bIsAvailable || CurrentItemType == EMTItemType::None || !OtherActor) return;

	UMTItemInventoryComponent* Inventory = OtherActor->FindComponentByClass<UMTItemInventoryComponent>();
	if (!Inventory) return;

	Inventory->GrantItem(CurrentItemType);

	bIsAvailable = false;
	ApplyVisualState();
	Multicast_PlayPickupEffects();
	StartRespawnTimer();
}

void AMTFieldItemBase::OnRep_CurrentItemType()
{
	ApplyVisualState();
}

void AMTFieldItemBase::OnRep_bIsAvailable()
{
	ApplyVisualState();
}

void AMTFieldItemBase::Multicast_PlayPickupEffects_Implementation()
{
	UMTItemData* Data = FindItemData(CurrentItemType);
	if (!Data) return;

	if (Data->PickupFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Data->PickupFX, GetActorLocation());
	}
	if (Data->PickupSound)
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), Data->PickupSound, GetActorLocation());
	}
}

void AMTFieldItemBase::ActivateWithRandomItem()
{
	if (EligibleItemTypes.Num() == 0) return;

	const int32 Index = FMath::RandHelper(EligibleItemTypes.Num());
	CurrentItemType = EligibleItemTypes[Index];
	bIsAvailable = (CurrentItemType != EMTItemType::None);
}

void AMTFieldItemBase::StartRespawnTimer()
{
	const float Delay = FMath::FRandRange(MinRespawnDelay, MaxRespawnDelay);
	GetWorldTimerManager().SetTimer(RespawnTimerHandle, this, &AMTFieldItemBase::RespawnItem, Delay, false);
}

void AMTFieldItemBase::RespawnItem()
{
	ActivateWithRandomItem();
	ApplyVisualState();
}

void AMTFieldItemBase::ApplyVisualState()
{
	if (bIsAvailable)
	{
		UMTItemData* Data = FindItemData(CurrentItemType);
		ItemMesh->SetStaticMesh(Data ? Data->ItemMesh : nullptr);
		ItemMesh->SetVisibility(true);
		CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	else
	{
		ItemMesh->SetVisibility(false);
		CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

UMTItemData* AMTFieldItemBase::FindItemData(EMTItemType Type) const
{
	if (AMTGameState* GS = GetWorld()->GetGameState<AMTGameState>())
	{
		return GS->GetItemData(Type);
	}
	return nullptr;
}
