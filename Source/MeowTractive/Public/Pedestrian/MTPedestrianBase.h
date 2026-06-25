// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Components/WidgetComponent.h"
#include "MTPedestrianBase.generated.h"

class APlayerState;

//행인 매료도 저장 구조체
USTRUCT(BlueprintType)
struct FPedestrianAttractiveness
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Pedestrian|Attractiveness")
	TObjectPtr<APlayerState> PlayerState = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Pedestrian|Attractiveness")
	float Value = 0.0f;

};

UCLASS()
class MEOWTRACTIVE_API AMTPedestrianBase : public ACharacter
{
	GENERATED_BODY()

public:
	AMTPedestrianBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Pedestrian|Attractiveness")
	float GetAttractiveness(APlayerState* TargetPlayerState) const;

	UFUNCTION(BlueprintCallable, Category = "Pedestrian|Attractiveness")
	void AddAttractiveness(APlayerState* TargetPlayerState, float Delta);

	UFUNCTION(Server, Reliable)
	void ServerAddAttractiveness(APlayerState* TargetPlayerState, float Delta);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(ReplicatedUsing = OnRep_Attractivenesses)
	TArray<FPedestrianAttractiveness> Attractivenesses;

	UFUNCTION()
	void OnRep_Attractivenesses();

	UPROPERTY(VisibleAnywhere, Category = "UI")
	UWidgetComponent* AttractivenessBarWidget;

	UPROPERTY(EditAnywhere, Category = "Pedestrian|UI")
	float AttractivenessBarVisibleDistance = 1500.f;

	float UIUpdateTimer = 0.f;

	UPROPERTY(EditAnywhere, Category = "Pedestrian|UI")
	float UIUpdateInterval = 0.1f; // 0.1초마다 체크

private:
	//걷기 속도
	UPROPERTY(EditAnywhere, Category = "Pedestrian|Movement")
	float WalkSpeed = 180.0f;

	//랜덤 지점 선택 반경
	UPROPERTY(EditAnywhere, Category = "Pedestrian|Movement")
	float WanderRadius = 700.0f;

	//목표지점 도착 판정 반경
	UPROPERTY(EditAnywhere, Category = "Pedestrian|Movement")
	float AcceptanceRadius = 80.0f;

	//최소 대기 시간
	UPROPERTY(EditAnywhere, Category = "Pedestrian|Movement")
	float MinWaitTime = 1.0f;

	//최대 대기 시간
	UPROPERTY(EditAnywhere, Category = "Pedestrian|Movement")
	float MaxWaitTime = 3.0f;

	// 이동 체크 간격
	UPROPERTY(EditAnywhere, Category = "Pedestrian|Movement")
	float MoveCheckDistance = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Pedestrian|Attractiveness")
	float MaxAttraction = 100.f;

	FTimerHandle WaitTimerHandle;

	FVector CurrentDestination = FVector::ZeroVector;

	bool bIsWaiting = false;
	bool bHasDestination = false;

	void MoveToRandomLocation();
	void WaitBeforeNextMove();
	void ResumeWander();
	bool HasReachedDestination() const;

};
