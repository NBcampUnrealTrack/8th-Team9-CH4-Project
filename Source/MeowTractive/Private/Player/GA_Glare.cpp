#include "Player/GA_Glare.h"

#include "Player/MTPlayerCharacter.h"
#include "Player/MTPlayerAttributeSet.h"
#include "Player/MTPlayerState.h"
#include "Game/MTGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagContainer.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

UGA_Glare::UGA_Glare()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	SetAssetTags(FGameplayTagContainer(MTGameplayTags::Ability::TAG_Skill_EyeBeam_Glare));

	// 스킬 상호배제: 시전 중 State.Casting 부여 + 동일 태그 시 발동 차단 (이동 스킬은 미부여라 예외)
	ActivationOwnedTags.AddTag(MTGameplayTags::State::TAG_State_Casting);
	ActivationBlockedTags.AddTag(MTGameplayTags::State::TAG_State_Casting);
	// 스킬 발동 시 진행 중인 기본공격(매료빔) 취소
	CancelAbilitiesWithTag.AddTag(MTGameplayTags::Ability::TAG_Skill_Attract_Beam);

	// 쿨다운 (공용 Cooldown GE + 스킬별 태그). 실제 초는 BP에서 튜닝.
	CooldownTags.AddTag(MTGameplayTags::Cooldown::TAG_Cooldown_EyeBeam_Glare);
	CooldownDuration = 6.f;
}

float UGA_Glare::GetEffectiveRange() const
{
	float Mul = 1.f;
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		Mul = ASC->GetNumericAttribute(UMTPlayerAttributeSet::GetEyeBeamRangeAttribute());
	}
	return BaseRange * FMath::Max(Mul, 0.f);
}

AActor* UGA_Glare::FindTargetCat(FVector& OutStart, FVector& OutEnd) const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	const APawn* Pawn = Cast<APawn>(Avatar);
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UWorld* World = GetWorld();
	if (!PC || !World)
	{
		return nullptr;
	}

	FVector CamLocation;
	FRotator CamRotation;
	PC->GetPlayerViewPoint(CamLocation, CamRotation);

	const FVector AimDirection = CamRotation.Vector();
	const float Range = GetEffectiveRange();
	const FVector CameraTraceEnd = CamLocation + AimDirection * Range;
	OutStart = GetGlareFXStartLocation();

	// 카메라 정면 LineTrace로 시각적 조준점을 계산한다. 월드 Trace에서는 Pawn을 제외한다.
	FCollisionQueryParams WorldParams;
	WorldParams.AddIgnoredActor(Avatar);
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		WorldParams.AddIgnoredActor(*It);
	}

	FVector CameraTarget = CameraTraceEnd;
	FHitResult CameraWorldHit;
	if (World->LineTraceSingleByChannel(
		CameraWorldHit, CamLocation, CameraTraceEnd, ECC_Visibility, WorldParams))
	{
		CameraTarget = CameraWorldHit.ImpactPoint;
	}

	// 카메라 정면의 Pawn은 반경 없는 LineTrace로 찾는다. 카메라와 플레이어 사이 Hit는 제외한다.
	FCollisionQueryParams CameraPawnParams;
	CameraPawnParams.AddIgnoredActor(Avatar);
	FCollisionObjectQueryParams PawnObjectParams;
	PawnObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	TArray<FHitResult> CameraPawnHits;
	World->LineTraceMultiByObjectType(
		CameraPawnHits, CamLocation, CameraTraceEnd, PawnObjectParams, CameraPawnParams);

	const float PlayerDepth = FVector::DotProduct(Avatar->GetActorLocation() - CamLocation, AimDirection);
	for (const FHitResult& Hit : CameraPawnHits)
	{
		if (!IsValid(Hit.GetActor()))
		{
			continue;
		}

		const float HitDepth = FVector::DotProduct(Hit.ImpactPoint - CamLocation, AimDirection);
		if (HitDepth <= CameraPlayerDepthTolerance
			|| HitDepth < PlayerDepth - CameraPlayerDepthTolerance)
		{
			continue;
		}

		if (Hit.Distance < FVector::Distance(CamLocation, CameraTarget))
		{
			CameraTarget = Hit.ImpactPoint;
		}
		break;
	}

	// 카메라 뒤의 조준점은 사용하지 않는다.
	OutEnd = FVector::DotProduct(CameraTarget - OutStart, AimDirection) > 0.f
		? CameraTarget
		: OutStart + AimDirection * Range;

	// 캐릭터 시작점에서 조준점까지 다시 LineTrace하여 실제 빔의 월드 관통을 막는다.
	FHitResult BeamWorldHit;
	if (World->LineTraceSingleByChannel(
		BeamWorldHit, OutStart, OutEnd, ECC_Visibility, WorldParams))
	{
		OutEnd = BeamWorldHit.ImpactPoint;
	}

	// 기존 데미지 판정은 Sphere Sweep이므로 유지한다. 판정 Hit로 시각적 끝점을 덮어쓰지 않는다.
	FCollisionQueryParams DamageParams;
	DamageParams.AddIgnoredActor(Avatar);
	FHitResult DamageHit;
	if (!World->SweepSingleByObjectType(
		DamageHit,
		OutStart,
		OutEnd,
		FQuat::Identity,
		PawnObjectParams,
		FCollisionShape::MakeSphere(TargetRadius),
		DamageParams))
	{
		return nullptr;
	}

	AActor* Target = DamageHit.GetActor();
	if (!AMTPlayerCharacter::IsEnemyCat(Avatar, Target))
	{
		return nullptr;
	}

	if (const AMTPlayerCharacter* TargetChar = Cast<AMTPlayerCharacter>(Target))
	{
		if (TargetChar->IsDead())
		{
			return nullptr;
		}
	}

	return Target;
}

//째려보기 소켓 있으면 위치 반환
FVector UGA_Glare::GetGlareFXStartLocation() const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (const AMTPlayerCharacter* Character = Cast<AMTPlayerCharacter>(Avatar))
	{
		if (const USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			if (Mesh->DoesSocketExist(GlareSocketName))
			{
				return Mesh->GetSocketLocation(GlareSocketName);
			}
		}
	}

	return Avatar ? Avatar->GetActorLocation() : FVector::ZeroVector;
}

//째려보기 큐 실행, 관련변수는 큐 파라미터로 전달.
void UGA_Glare::ExecuteGlareGameplayCue(const FVector& Start, const FVector& End) const
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!ASC || !Avatar)
	{
		return;
	}

	FGameplayCueParameters CueParams;
	CueParams.Location = End;
	CueParams.Instigator = Avatar;
	CueParams.EffectCauser = Avatar;

	if (const APawn* AvatarPawn = Cast<APawn>(Avatar))
	{
		CueParams.SourceObject = AvatarPawn->GetPlayerState<AMTPlayerState>();
	}

	// FGameplayCueParameters에는 임의 Vector 슬롯이 하나뿐이므로 TraceStart/TraceEnd는 HitResult로 전달한다.
	FGameplayEffectContextHandle CueContext = ASC->MakeEffectContext();
	FHitResult CueTrace;
	CueTrace.TraceStart = Start;	//시작지점
	CueTrace.TraceEnd = End;	//끝지점
	CueTrace.Location = End;
	CueTrace.ImpactPoint = End;
	CueContext.AddHitResult(CueTrace, true);
	CueParams.EffectContext = CueContext;

	//큐 실행
	ASC->ExecuteGameplayCue(
		FGameplayTag::RequestGameplayTag(FName("GameplayCue.Cat.Glare")),
		CueParams);
}

void UGA_Glare::ActivateAbility(
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

	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	AActor* Target = FindTargetCat(Start, End);
	const bool bHit = (Target != nullptr);

	if (bDrawDebug)
	{
		DrawDebugLine(GetWorld(), Start, End, bHit ? FColor::Red : FColor::Green, false, 0.5f, 0, 2.f);
	}

	OnGlare(Start, End, bHit); // 연출 훅 (예측 클라/서버 모두)
	ExecuteGlareGameplayCue(Start, End);

	// 데미지/슬로우 적용은 서버 권위
	if (bHit && HasAuthority(&ActivationInfo))
	{
		UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
		if (SourceASC && TargetASC)
		{
			const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"));
			const FGameplayTag DurationTag = FGameplayTag::RequestGameplayTag(FName("Data.Duration"));

			if (DamageEffect)
			{
				FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
				Ctx.AddSourceObject(GetAvatarActorFromActorInfo());
				FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(DamageEffect, GetAbilityLevel(), Ctx);
				if (Spec.IsValid())
				{
					Spec.Data->SetSetByCallerMagnitude(DamageTag, Damage);
					SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
				}
			}

			if (SlowEffect)
			{
				FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
				Ctx.AddSourceObject(GetAvatarActorFromActorInfo());
				FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(SlowEffect, GetAbilityLevel(), Ctx);
				if (Spec.IsValid())
				{
					Spec.Data->SetSetByCallerMagnitude(DurationTag, SlowDuration);
					Spec.Data->SetSetByCallerMagnitude(
						FGameplayTag::RequestGameplayTag(FName("Data.SlowMult")), SlowMultiplier);
					SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
				}
			}
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
