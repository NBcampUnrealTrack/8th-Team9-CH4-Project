#include "Player/GA_HeartBeam.h"

#include "Player/MTPlayerCharacter.h"
#include "Player/MTPlayerAttributeSet.h"
#include "Player/MTPlayerState.h"
#include "Pedestrian/MTPedestrianBase.h"
#include "Game/MTGameplayTags.h"
#include "Game/MTLog.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"

UGA_HeartBeam::UGA_HeartBeam()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	SetAssetTags(FGameplayTagContainer(MTGameplayTags::Ability::TAG_Skill_EyeBeam_HeartBeam));

	// 스킬 상호배제: 시전 중 State.Casting 부여 + 동일 태그 시 발동 차단 (이동 스킬은 미부여라 예외)
	ActivationOwnedTags.AddTag(MTGameplayTags::State::TAG_State_Casting);
	ActivationBlockedTags.AddTag(MTGameplayTags::State::TAG_State_Casting);
	// 스킬 발동 시 진행 중인 기본공격(매료빔) 취소
	CancelAbilitiesWithTag.AddTag(MTGameplayTags::Ability::TAG_Skill_Attract_Beam);

	// 쿨다운 (공용 Cooldown GE + 스킬별 태그). 실제 초는 BP에서 튜닝.
	CooldownTags.AddTag(MTGameplayTags::Cooldown::TAG_Cooldown_EyeBeam_HeartBeam);
	CooldownDuration = 12.f;
}

float UGA_HeartBeam::GetEffectiveRange() const
{
	float Mul = 1.f;
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		Mul = ASC->GetNumericAttribute(UMTPlayerAttributeSet::GetEyeBeamRangeAttribute());
	}
	return BaseRange * FMath::Max(Mul, 0.f);
}

void UGA_HeartBeam::ActivateAbility(
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

	StartHeartBeamFX();
	OnBeamStart();

	FireBeam(); // 첫 발 즉시
	bIsBeamVisualUpdateActive = true;
	UpdateBeamVisual();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			BeamTimerHandle, this, &UGA_HeartBeam::FireBeam, FireInterval, /*bLoop=*/true);
		// 5초 후 자동 종료
		World->GetTimerManager().SetTimer(
			DurationTimerHandle, this, &UGA_HeartBeam::OnDurationElapsed, Duration, /*bLoop=*/false);
	}
}

void UGA_HeartBeam::OnDurationElapsed()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_HeartBeam::FireBeam()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	FVector Start;
	FVector BeamEnd;
	bool bHitWorld = false;
	if (!TraceBeam(Start, BeamEnd, bHitWorld))
	{
		return;
	}

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Avatar);

	if (bDrawDebug)
	{
		DrawDebugLine(GetWorld(), Start, BeamEnd, FColor::Magenta, false, FireInterval, 0, 3.f);
	}

	UpdateHeartBeamFX(Start, BeamEnd);
	OnBeamUpdate(Start, BeamEnd);

	// 적용은 서버 권위. 경로상 '전원'(관통) 판정.
	if (!HasAuthority(&CurrentActivationInfo))
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC)
	{
		return;
	}

	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);
	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByObjectType(
		Hits, Start, BeamEnd, FQuat::Identity, ObjParams, FCollisionShape::MakeSphere(BeamRadius), Params);

	const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"));
	const float AttractTick = AttractPerSecond * FireInterval;
	const float DamageTick = DamagePerSecond * FireInterval;

	TSet<AActor*> Processed; // 스윕 다중 히트 중복 방지
	for (const FHitResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		if (!Target || Processed.Contains(Target))
		{
			continue;
		}
		Processed.Add(Target);

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
		if (!TargetASC)
		{
			continue;
		}

		// 행인 → 매료
		if (Target->IsA(AMTPedestrianBase::StaticClass()))
		{
			if (AttractEffect && AttractTick > 0.f)
			{
				FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
				Ctx.AddSourceObject(Avatar);
				FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(AttractEffect, GetAbilityLevel(), Ctx);
				if (Spec.IsValid())
				{
					Spec.Data->SetSetByCallerMagnitude(DamageTag, AttractTick);
					SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
				}
			}
			continue;
		}

		// 적 고양이 → 데미지
		if (AMTPlayerCharacter::IsEnemyCat(Avatar, Target))
		{
			if (const AMTPlayerCharacter* TargetChar = Cast<AMTPlayerCharacter>(Target))
			{
				if (TargetChar->IsDead())
				{
					continue;
				}
			}
			if (DamageEffect && DamageTick > 0.f)
			{
				FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
				Ctx.AddSourceObject(Avatar);
				FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(DamageEffect, GetAbilityLevel(), Ctx);
				if (Spec.IsValid())
				{
					Spec.Data->SetSetByCallerMagnitude(DamageTag, DamageTick);
					SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
				}
			}
		}
	}
}

//빔 VFX 업데이트 호출용 래퍼 함수
void UGA_HeartBeam::UpdateBeamVisual()
{
	FVector Start;
	FVector End;
	bool bHitWorld = false;
	if (TraceBeam(Start, End, bHitWorld))
	{
		UpdateHeartBeamFX(Start, End);
		OnBeamUpdate(Start, End);
	}

	if (bIsBeamVisualUpdateActive)
	{
		if (UWorld* World = GetWorld())
		{
			BeamVisualTimerHandle = World->GetTimerManager().SetTimerForNextTick(
				this, &UGA_HeartBeam::UpdateBeamVisual);
		}
	}
}

void UGA_HeartBeam::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	bIsBeamVisualUpdateActive = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BeamTimerHandle);
		World->GetTimerManager().ClearTimer(DurationTimerHandle);
		World->GetTimerManager().ClearTimer(BeamVisualTimerHandle);
		World->GetTimerManager().ClearTimer(BeamFXFadeInTimerHandle);
	}

	StopHeartBeamFX();
	OnBeamEnd();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

//하트빔 VFX 시작
void UGA_HeartBeam::StartHeartBeamFX()
{
	if (!HeartBeamFX)
	{
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	USceneComponent* AttachComponent = Avatar ? Avatar->GetRootComponent() : nullptr;
	if (const ACharacter* Character = Cast<ACharacter>(Avatar))
	{
		if (Character->GetMesh())
		{
			AttachComponent = Character->GetMesh();
		}
	}

	if (BeamLoopSound)
	{
		if (Avatar)
		{
			BeamAudioComponent = UGameplayStatics::SpawnSoundAttached(
				BeamLoopSound, Avatar->GetRootComponent(),
				NAME_None, FVector::ZeroVector, EAttachLocation::SnapToTarget, true);
		}
	}

	if (!AttachComponent)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BeamFXCleanupTimerHandle);
		World->GetTimerManager().ClearTimer(BeamFXFadeInTimerHandle);
	}

	if (!ActiveHeartBeamFX)
	{
		ActiveHeartBeamFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
			HeartBeamFX, AttachComponent, HeartBeamSocketName,
			FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget, false);
	}
	else
	{
		ActiveHeartBeamFX->Activate(true);
	}

	if (!ActiveHeartBeamFX)
	{
		return;
	}

	bIsHeartBeamFXEnding = false;
	ActiveHeartBeamFX->SetVariableBool(FName(TEXT("User.BeamStarting")), true);
	ActiveHeartBeamFX->SetVariableBool(FName(TEXT("User.BeamEnding")), false);
	ActiveHeartBeamFX->SetVariableFloat(FName(TEXT("User.BeamFadeInTime")), BeamFXFadeInTime);
	ActiveHeartBeamFX->SetVariableFloat(FName(TEXT("User.BeamFadeOutTime")), BeamFXFadeOutTime);
	ActiveHeartBeamFX->SetVariableLinearColor(FName(TEXT("User.PlayerColor")), GetAvatarPlayerColor());

	if (BeamFXFadeInTime <= 0.f)
	{
		FinishHeartBeamFadeIn();
	}
	else if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			BeamFXFadeInTimerHandle, this, &UGA_HeartBeam::FinishHeartBeamFadeIn,
			BeamFXFadeInTime, false);
	}
}

//하트빔 스케일/페이드 인 종료 알림
void UGA_HeartBeam::FinishHeartBeamFadeIn()
{
	if (ActiveHeartBeamFX)
	{
		ActiveHeartBeamFX->SetVariableBool(FName(TEXT("User.BeamStarting")), false);
	}
}

//하트빔 VFX 업데이트
void UGA_HeartBeam::UpdateHeartBeamFX(const FVector& Start, const FVector& End)
{
	if (ActiveHeartBeamFX)
	{
		ActiveHeartBeamFX->SetVariableVec3(FName(TEXT("User.AbsoluteBeamStart")), Start);
		ActiveHeartBeamFX->SetVariableVec3(FName(TEXT("User.AbsoluteBeamEnd")), End);
	}
}

//하트빔 VFX 스케일/페이드 아웃 시작
void UGA_HeartBeam::StopHeartBeamFX()
{
	if (!ActiveHeartBeamFX || bIsHeartBeamFXEnding)
	{
		return;
	}

	bIsHeartBeamFXEnding = true;
	ActiveHeartBeamFX->SetVariableBool(FName(TEXT("User.BeamStarting")), false);
	ActiveHeartBeamFX->SetVariableBool(FName(TEXT("User.BeamEnding")), true);
	ActiveHeartBeamFX->SetVariableFloat(FName(TEXT("User.BeamFadeOutTime")), BeamFXFadeOutTime);

	if (BeamAudioComponent)
	{
		BeamAudioComponent->FadeOut(0.3f, 0.f);
		BeamAudioComponent = nullptr;
	}

	if (BeamFXFadeOutTime <= 0.f)
	{
		FinishHeartBeamFX();
	}

	else if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			BeamFXCleanupTimerHandle, this, &UGA_HeartBeam::FinishHeartBeamFX,
			BeamFXFadeOutTime, false);
	}
}

//하트빔 스케일/페이드 아웃 종료, VFX 제거.
void UGA_HeartBeam::FinishHeartBeamFX()
{
	if (ActiveHeartBeamFX)
	{
		ActiveHeartBeamFX->Deactivate();
		ActiveHeartBeamFX->DestroyComponent();
		ActiveHeartBeamFX = nullptr;
	}
	bIsHeartBeamFXEnding = false;
}

//VFX용 소켓 있으면 위치 확인해서 반환
FVector UGA_HeartBeam::GetHeartBeamFXStartLocation() const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (const ACharacter* Character = Cast<ACharacter>(Avatar))
	{
		if (const USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			if (Mesh->DoesSocketExist(HeartBeamSocketName))
			{
				return Mesh->GetSocketLocation(HeartBeamSocketName);
			}
		}
	}
	return Avatar ? Avatar->GetActorLocation() : FVector::ZeroVector;
}

FLinearColor UGA_HeartBeam::GetAvatarPlayerColor() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const APawn* Pawn = Cast<APawn>(Avatar);
	const AMTPlayerState* MTPlayerState = Pawn ? Pawn->GetPlayerState<AMTPlayerState>() : nullptr;
	return MTPlayerState ? MTPlayerState->GetTeamColor() : FLinearColor::White;
}

//조준점 산정, 플레이어와 카메라 사이에 있는 장애물은 무시. 추후 GA_AttractiveBeam 조준과 함께 라이브러리 분리 필요.
bool UGA_HeartBeam::TraceBeam(FVector& OutTraceStart, FVector& OutBeamEnd, bool& bOutHitWorld) const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	const APawn* Pawn = Cast<APawn>(Avatar);
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UWorld* World = GetWorld();
	if (!PC || !World)
	{
		return false;
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
	const FVector AimDirection = CameraRotation.Vector();
	const float Range = GetEffectiveRange();
	const FVector CameraTraceEnd = CameraLocation + AimDirection * Range;
	OutTraceStart = GetHeartBeamFXStartLocation();

	FCollisionQueryParams WorldParams;
	WorldParams.AddIgnoredActor(Avatar);
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		WorldParams.AddIgnoredActor(*It);
	}

	FVector CameraTarget = CameraTraceEnd;
	FHitResult CameraHit;
	if (World->LineTraceSingleByChannel(CameraHit, CameraLocation, CameraTraceEnd, ECC_Visibility, WorldParams))
	{
		CameraTarget = CameraHit.ImpactPoint;
	}

	// 카메라 정면의 Pawn도 반경 없는 LineTrace로 검출한다.
	// 카메라와 플레이어 사이 Hit는 조준 대상에서 제외한다.
	FCollisionQueryParams CameraPawnParams;
	CameraPawnParams.AddIgnoredActor(Avatar);
	FCollisionObjectQueryParams PawnObjectParams;
	PawnObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	TArray<FHitResult> CameraPawnHits;
	World->LineTraceMultiByObjectType(
		CameraPawnHits, CameraLocation, CameraTraceEnd, PawnObjectParams, CameraPawnParams);

	const float PlayerDepth = FVector::DotProduct(Avatar->GetActorLocation() - CameraLocation, AimDirection);
	for (const FHitResult& Hit : CameraPawnHits)
	{
		if (!IsValid(Hit.GetActor()))
		{
			continue;
		}

		const float HitDepth = FVector::DotProduct(Hit.ImpactPoint - CameraLocation, AimDirection);
		if (HitDepth <= CameraPlayerDepthTolerance
			|| HitDepth < PlayerDepth - CameraPlayerDepthTolerance)
		{
			continue;
		}

		if (Hit.Distance < FVector::Distance(CameraLocation, CameraTarget))
		{
			CameraTarget = Hit.ImpactPoint;
		}
		break;
	}

	OutBeamEnd = FVector::DotProduct(CameraTarget - OutTraceStart, AimDirection) > 0.f
		? CameraTarget
		: OutTraceStart + AimDirection * Range;

	FHitResult WorldHit;
	bOutHitWorld = World->LineTraceSingleByChannel(
		WorldHit, OutTraceStart, OutBeamEnd, ECC_Visibility, WorldParams);
	if (bOutHitWorld)
	{
		OutBeamEnd = WorldHit.ImpactPoint;
	}
	return true;
}
