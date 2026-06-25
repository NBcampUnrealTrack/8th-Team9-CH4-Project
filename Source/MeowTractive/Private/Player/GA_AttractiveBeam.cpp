#include "Player/GA_AttractiveBeam.h"

#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"

void UGA_AttractiveBeam::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* Avatar = GetAvatarActorFromActorInfo();
	APawn* Pawn = Cast<APawn>(Avatar);
	if (!Pawn)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 카메라 시점 가져오기
	FVector CamLocation;
	FRotator CamRotation;
	PC->GetPlayerViewPoint(CamLocation, CamRotation);

	const FVector Start = CamLocation;
	const FVector End = Start + (CamRotation.Vector() * TraceDistance);

	// Pawn 채널 단일 라인 트레이스
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Avatar); // 자기 자신 제외

	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_Pawn); // Pawn 오브젝트 타입을 타겟

	bool bHit = GetWorld()->LineTraceSingleByObjectType(
		Hit,
		Start,
		End,
		ObjParams,
		Params
	);

	if (bHit && Hit.GetActor())
	{
		AActor* TargetActor = Hit.GetActor();

		// 멈춘 지점까지만 빨간 선 + 타격 지점 표시
		DrawDebugLine(GetWorld(), Start, Hit.ImpactPoint, FColor::Red, false, 2.f, 0, 1.f);
		DrawDebugPoint(GetWorld(), Hit.ImpactPoint, 12.f, FColor::Yellow, false, 2.f);

		// 화면에 NPC 이름 출력
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1, 2.f, FColor::Green,
				FString::Printf(TEXT("Hit NPC: %s"), *TargetActor->GetName()));
		}

		UE_LOG(LogTemp, Warning, TEXT("AttractiveBeam Hit: %s"), *TargetActor->GetName());
	}
	else
	{
		// 안 맞았으면 끝까지 초록 선
		DrawDebugLine(GetWorld(), Start, End, FColor::Green, false, 2.f, 0, 1.f);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
