#include "Player/GA_AttractiveBeam.h"

#include "Pedestrian/MTPedestrianBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "DrawDebugHelpers.h"

UGA_AttractiveBeam::UGA_AttractiveBeam()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// 클라 예측 + 서버 실행: 빔/디버그는 즉시, 실제 매료 적용은 서버에서.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_AttractiveBeam::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	const APawn* Pawn = Cast<APawn>(Avatar);
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 카메라 시점 기준 라인트레이스 (Pawn 오브젝트 타입)
	FVector CamLocation;
	FRotator CamRotation;
	PC->GetPlayerViewPoint(CamLocation, CamRotation);
	const FVector Start = CamLocation;
	const FVector End = Start + (CamRotation.Vector() * TraceDistance);

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Avatar);
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);

	const bool bHit = GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, ObjParams, Params);

	if (bDrawDebug)
	{
		if (bHit)
		{
			DrawDebugLine(GetWorld(), Start, Hit.ImpactPoint, FColor::Red, false, 2.f, 0, 1.f);
			DrawDebugPoint(GetWorld(), Hit.ImpactPoint, 12.f, FColor::Yellow, false, 2.f);
		}
		else
		{
			DrawDebugLine(GetWorld(), Start, End, FColor::Green, false, 2.f, 0, 1.f);
		}
	}

	// 실제 매료도 적용은 서버 권위 + 행인 대상만 (기여도 정확성)
	if (bHit && HasAuthority(&ActivationInfo))
	{
		if (AMTPedestrianBase* Ped = Cast<AMTPedestrianBase>(Hit.GetActor()))
		{
			ApplyAttractiveDamage(Ped);
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UGA_AttractiveBeam::ApplyAttractiveDamage(AMTPedestrianBase* Target)
{
	if (!Target || !AttractiveDamageGE)
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	if (!SourceASC || !TargetASC)
	{
		return;
	}

	// 소스(고양이) ASC로 만들어야 instigator=고양이 → 행인 PostGEExecute가 가해자를 정확히 기록
	FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
	Ctx.AddSourceObject(GetAvatarActorFromActorInfo());
	const FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(AttractiveDamageGE, GetAbilityLevel(), Ctx);
	if (Spec.IsValid())
	{
		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	}
}
