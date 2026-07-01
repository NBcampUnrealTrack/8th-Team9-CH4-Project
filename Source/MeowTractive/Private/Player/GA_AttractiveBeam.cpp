#include "Player/GA_AttractiveBeam.h"

#include "Pedestrian/MTPedestrianBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagContainer.h"
#include "DrawDebugHelpers.h"

UGA_AttractiveBeam::UGA_AttractiveBeam()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// 클라 예측 + 서버 실행: 빔/디버그는 즉시, 실제 매료 적용은 서버에서.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_AttractiveBeam::FireBeam()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	const APawn* Pawn = Cast<APawn>(Avatar);
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC)
	{
		return;
	};

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
			DrawDebugLine(GetWorld(), Start, Hit.ImpactPoint, FColor::Red, false, FireInterval, 0, 1.f);
			DrawDebugPoint(GetWorld(), Hit.ImpactPoint, 12.f, FColor::Yellow, false, FireInterval);
		}
		else
		{
			DrawDebugLine(GetWorld(), Start, End, FColor::Green, false, FireInterval, 0, 1.f);
		}
	}

	if (bHit && HasAuthority(&CurrentActivationInfo))
	{
		if (AMTPedestrianBase* Ped = Cast<AMTPedestrianBase>(Hit.GetActor()))
		{
			ApplyAttractiveDamage(Ped);
		}
	}
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

	// 첫 발 즉시
	FireBeam();

	// 이후 FireInterval 간격으로 반복
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			BeamTimerHandle, this, &UGA_AttractiveBeam::FireBeam, FireInterval, /*bLoop=*/true);
	}
	// EndAbility 호출 제거 — 입력이 떨어질 때까지 살아있음
}

void UGA_AttractiveBeam::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BeamTimerHandle);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_AttractiveBeam::ApplyAttractiveDamage(AMTPedestrianBase* Target)
{
	// --- GE 호출전 정보 준비 ---
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
	FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(AttractiveDamageGE, GetAbilityLevel(), Ctx);

	if (Spec.IsValid())
	{
		const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"));
		Spec.Data->SetSetByCallerMagnitude(DamageTag, BaseDamage);
		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	}
}
