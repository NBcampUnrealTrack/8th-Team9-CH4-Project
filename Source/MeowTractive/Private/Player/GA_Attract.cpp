#include "Player/GA_Attract.h"

#include "Pedestrian/MTPedestrianBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"

UGA_Attract::UGA_Attract()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// 클라 예측 활성 + 서버 실행: 빔 VFX는 즉시, 실제 매료 적용은 서버에서.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_Attract::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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
	if (!Avatar)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 조준 방향: 컨트롤러 뷰포인트 우선, 없으면 액터 전방
	FVector Start = Avatar->GetActorLocation();
	FRotator ViewRot = Avatar->GetActorRotation();
	if (const APawn* Pawn = Cast<APawn>(Avatar))
	{
		if (AController* Ctrl = Pawn->GetController())
		{
			Ctrl->GetPlayerViewPoint(Start, ViewRot);
		}
	}
	const FVector End = Start + ViewRot.Vector() * BeamRange;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Avatar);
	const bool bHit = Avatar->GetWorld()->LineTraceSingleByChannel(Hit, Start, End, TraceChannel, Params);

	// 빔 VFX 훅 (코스메틱 — 실행되는 인스턴스에서)
	OnBeamFired(Start, bHit ? Hit.ImpactPoint : End, bHit);

	// 실제 매료도 적용은 서버 권위에서만 (기여도 정확성)
	if (bHit && HasAuthority(&ActivationInfo))
	{
		if (AMTPedestrianBase* Ped = Cast<AMTPedestrianBase>(Hit.GetActor()))
		{
			ApplyAttractiveDamage(Ped);
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UGA_Attract::ApplyAttractiveDamage(AMTPedestrianBase* Target)
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

	// 소스(고양이) ASC로 만들어야 instigator=고양이 → 행인 기여도가 올바르게 기록됨
	FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
	Ctx.AddSourceObject(GetAvatarActorFromActorInfo());
	const FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(AttractiveDamageGE, GetAbilityLevel(), Ctx);
	if (Spec.IsValid())
	{
		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	}
}
