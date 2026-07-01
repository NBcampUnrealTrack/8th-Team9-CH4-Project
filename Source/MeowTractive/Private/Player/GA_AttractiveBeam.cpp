#include "Player/GA_AttractiveBeam.h"

#include "Pedestrian/MTPedestrianBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagContainer.h"
#include "Engine/World.h"
#include "TimerManager.h"
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
	}

	FVector CamLocation;
	FRotator CamRotation;
	PC->GetPlayerViewPoint(CamLocation, CamRotation);
	const FVector Start = CamLocation;
	const FVector TraceEnd = Start + (CamRotation.Vector() * TraceDistance);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Avatar);

	// 빔 시각 끝점: 벽(가시성)에 막히면 거기까지
	FVector BeamEnd = TraceEnd;
	FHitResult WallHit;
	if (GetWorld()->LineTraceSingleByChannel(WallHit, Start, TraceEnd, ECC_Visibility, Params))
	{
		BeamEnd = WallHit.ImpactPoint;
	}

	// 굵은 빔: 구체 스윕 단일 판정 — 경로상 '첫 폰'만 (관통 없음)
	FHitResult PawnHit;
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);
	const bool bPawnHit = GetWorld()->SweepSingleByObjectType(
		PawnHit, Start, BeamEnd, FQuat::Identity, ObjParams, FCollisionShape::MakeSphere(BeamRadius), Params);

	AMTPedestrianBase* Ped = nullptr;
	if (bPawnHit)
	{
		// 폰에 맞으면 빔은 거기서 끝 (관통 X)
		BeamEnd = PawnHit.ImpactPoint;
		Ped = Cast<AMTPedestrianBase>(PawnHit.GetActor());
	}

	const bool bHitPed = (Ped != nullptr);
	if (bHitPed && HasAuthority(&CurrentActivationInfo))
	{
		ApplyAttractiveDamage(Ped);
	}

	if (bDrawDebug)
	{
		DrawDebugLine(GetWorld(), Start, BeamEnd, bHitPed ? FColor::Red : FColor::Green, false, FireInterval, 0, 2.f);
		DrawDebugSphere(GetWorld(), BeamEnd, BeamRadius, 8, FColor::Yellow, false, FireInterval);
	}

	// VFX 훅 — BP가 Niagara 빔 끝점을 여기에 맞춤
	OnBeamUpdate(Start, BeamEnd, bHitPed);
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

	OnBeamStart(); // VFX 시작 훅

	// 첫 발 즉시
	FireBeam();

	// 이후 FireInterval 간격으로 반복 (지속 빔)
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

	OnBeamEnd(); // VFX 종료 훅

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
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
	FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(AttractiveDamageGE, GetAbilityLevel(), Ctx);

	if (Spec.IsValid())
	{
		const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"));
		Spec.Data->SetSetByCallerMagnitude(DamageTag, BaseDamage);
		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	}
}
