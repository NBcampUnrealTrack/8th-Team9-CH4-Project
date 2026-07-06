// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MTItemTypes.h"
#include "MTFieldItemBase.generated.h"

class USphereComponent;
class UMTItemData;

UCLASS()
class MEOWTRACTIVE_API AMTFieldItemBase : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AMTFieldItemBase();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(ReplicatedUsing = OnRep_CurrentItemType)
	EMTItemType CurrentItemType = EMTItemType::None;

	UFUNCTION()
	void OnRep_CurrentItemType();

	UPROPERTY(ReplicatedUsing = OnRep_bIsAvailable)
	bool bIsAvailable = false;

	UFUNCTION()
	void OnRep_bIsAvailable();

	// ---- 1회성 이벤트 (Multicast) ----

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayPickupEffects();

	// ---- 서버 전용 로직 ----

	void ActivateWithRandomItem();
	void StartRespawnTimer();
	void RespawnItem();
	void ApplyVisualState();

	UPROPERTY(VisibleAnywhere)
	USphereComponent* CollisionSphere;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* ItemMesh;

	// 이 스폰 포인트에서 등장 가능한 아이템 타입들. 에디터에서 채워 넣습니다.
	UPROPERTY(EditDefaultsOnly, Category = "Item")
	TArray<EMTItemType> EligibleItemTypes;

	UPROPERTY(EditDefaultsOnly, Category = "Item")
	float MinRespawnDelay = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "Item")
	float MaxRespawnDelay = 15.f;

	FTimerHandle RespawnTimerHandle;

	UMTItemData* FindItemData(EMTItemType Type) const;

};
