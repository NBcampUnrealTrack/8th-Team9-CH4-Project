#include "Player/GA_AttractiveBeam.h"

#include "Pedestrian/MTPedestrianBase.h"
#include "Game/MTGameplayTags.h"
#include "Player/MTPlayerState.h"
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
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"

UGA_AttractiveBeam::UGA_AttractiveBeam()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// 클라 예측 + 서버 실행: 빔/디버그는 즉시, 실제 매료 적용은 서버에서.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	SetAssetTags(FGameplayTagContainer(MTGameplayTags::Ability::TAG_Skill_Attract_Beam));

	// 기본공격: State.Casting 미부여(스킬을 막지 않음). 스킬 시전 중엔 발동만 차단 + 스킬이 이 어빌리티를 취소.
	ActivationBlockedTags.AddTag(MTGameplayTags::State::TAG_State_Casting);
}

//빔 발사
void UGA_AttractiveBeam::FireBeam()
{
	FVector TraceStart;
	FVector BeamEnd;
	AMTPedestrianBase* Ped = nullptr;
	bool bHitActor = false;
	bool bHitWorld = false;
	const bool bTraceValid = TraceBeam(TraceStart, BeamEnd, Ped, bHitActor, bHitWorld);
	UpdateAttractiveBeamHitFX(bTraceValid && (bHitActor || bHitWorld));
	if (!bTraceValid)
	{
		return;
	}


	const bool bHitPed = (Ped != nullptr);
	if (bHitPed && HasAuthority(&CurrentActivationInfo))
	{
		ApplyAttractiveDamage(Ped);
	}

	if (bDrawDebug)
	{
		DrawDebugLine(GetWorld(), TraceStart, BeamEnd, bHitPed ? FColor::Red : FColor::Green, false, FireInterval, 0, 2.f);
		DrawDebugSphere(GetWorld(), BeamEnd, BeamRadius, 8, FColor::Yellow, false, FireInterval);
	}

	UpdateAttractiveBeamFX(TraceStart, BeamEnd);
	OnBeamUpdate(TraceStart, BeamEnd, bHitPed);
}

//이펙트만 업데이트
void UGA_AttractiveBeam::UpdateBeamVisual()
{
	FVector TraceStart;
	FVector BeamEnd;
	AMTPedestrianBase* Ped = nullptr;
	bool bHitActor = false;
	bool bHitWorld = false;
	const bool bTraceValid = TraceBeam(TraceStart, BeamEnd, Ped, bHitActor, bHitWorld);
	UpdateAttractiveBeamHitFX(bTraceValid && (bHitActor || bHitWorld));
	if (bTraceValid)
	{
		const bool bHitPed = (Ped != nullptr);
		UpdateAttractiveBeamFX(TraceStart, BeamEnd);
		OnBeamUpdate(TraceStart, BeamEnd, bHitPed);
	}

	if (bIsBeamVisualUpdateActive)
	{
		if (UWorld* World = GetWorld())
		{
			BeamVisualTimerHandle = World->GetTimerManager().SetTimerForNextTick(
				this, &UGA_AttractiveBeam::UpdateBeamVisual);
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

	StartAttractiveBeamFX();
	OnBeamStart(); // VFX 시작 훅
	if (BeamLoopSound)
	{
		AActor* Avatar = GetAvatarActorFromActorInfo();
		if (Avatar)
		{
			BeamAudioComponent = UGameplayStatics::SpawnSoundAttached(
				BeamLoopSound, Avatar->GetRootComponent(),
				NAME_None, FVector::ZeroVector, EAttachLocation::SnapToTarget, true);
		}
	}

	// 첫 발 즉시
	FireBeam();
	bIsBeamVisualUpdateActive = true;
	UpdateBeamVisual();

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
	bIsBeamVisualUpdateActive = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BeamTimerHandle);
		World->GetTimerManager().ClearTimer(BeamVisualTimerHandle);
	}

	StopAttractiveBeamFX();
	OnBeamEnd(); // VFX 종료 훅
	if (BeamAudioComponent)
	{
		BeamAudioComponent->Stop();
		BeamAudioComponent = nullptr;
	}

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

// 매료빔 VFX 시작
void UGA_AttractiveBeam::StartAttractiveBeamFX()
{
	if (!AttractiveBeamFX)
	{
		return;
	}

	if (ActiveAttractiveBeamFX)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(BeamFXCleanupTimerHandle);
			World->GetTimerManager().ClearTimer(BeamFXFadeInTimerHandle);
		}

		bIsAttractiveBeamFXEnding = false;
		ActiveAttractiveBeamFX->Activate(true);
	}
	else
	{
		AActor* Avatar = GetAvatarActorFromActorInfo();
		if (!Avatar)
		{
			return;
		}

		USceneComponent* AttachComponent = Avatar->GetRootComponent();
		if (const ACharacter* Character = Cast<ACharacter>(Avatar))
		{
			if (Character->GetMesh())
			{
				AttachComponent = Character->GetMesh();
			}
		}

		if (!AttachComponent)
		{
			return;
		}

		UNiagaraSystem* AttractiveBeamFXAsset = AttractiveBeamFX.Get();
		if (!AttractiveBeamFXAsset)
		{
			return;
		}

		ActiveAttractiveBeamFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
			AttractiveBeamFXAsset,
			AttachComponent,
			AttractiveBeamSocketName,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget,
			false);
	}

	if (!ActiveAttractiveBeamFX)
	{
		return;
	}

	bIsAttractiveBeamFXEnding = false;
	ActiveAttractiveBeamFX->SetVariableBool(FName(TEXT("User.BeamStarting")), true);
	ActiveAttractiveBeamFX->SetVariableBool(FName(TEXT("User.BeamEnding")), false);
	ActiveAttractiveBeamFX->SetVariableFloat(FName(TEXT("User.BeamFadeInTime")), BeamFXFadeInTime);
	ActiveAttractiveBeamFX->SetVariableFloat(FName(TEXT("User.BeamFadeOutTime")), BeamFXFadeOutTime);
	ActiveAttractiveBeamFX->SetVariableLinearColor(FName(TEXT("User.PlayerColor")), GetAvatarPlayerColor());

	if (BeamFXFadeInTime <= 0.f)
	{
		FinishAttractiveBeamFadeIn();
	}
	else if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			BeamFXFadeInTimerHandle,
			this,
			&UGA_AttractiveBeam::FinishAttractiveBeamFadeIn,
			BeamFXFadeInTime,
			false);
	}
}

//VFX 변수 업데이트
void UGA_AttractiveBeam::UpdateAttractiveBeamFX(const FVector& Start, const FVector& End)
{
	if (!ActiveAttractiveBeamFX)
	{
		return;
	}

	ActiveAttractiveBeamFX->SetVariableVec3(FName(TEXT("User.AbsoluteBeamStart")), Start);
	ActiveAttractiveBeamFX->SetVariableVec3(FName(TEXT("User.AbsoluteBeamEnd")), End);
	ActiveAttractiveBeamFX->SetVariableLinearColor(FName(TEXT("User.PlayerColor")), GetAvatarPlayerColor());
}

//빔 적중 여부만 VFX 변수에 업데이트
void UGA_AttractiveBeam::UpdateAttractiveBeamHitFX(bool bHitActor)
{
	if (!ActiveAttractiveBeamFX)
	{
		return;
	}
	ActiveAttractiveBeamFX->SetVariableBool(FName(TEXT("User.TraceHit")), bHitActor);
}


//VFX 스케일/페이드 아웃 시작 알림.
void UGA_AttractiveBeam::StopAttractiveBeamFX()
{
	if (!ActiveAttractiveBeamFX || bIsAttractiveBeamFXEnding)
	{
		return;
	}

	bIsAttractiveBeamFXEnding = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BeamFXFadeInTimerHandle);
	}
	ActiveAttractiveBeamFX->SetVariableBool(FName(TEXT("User.BeamStarting")), false);
	ActiveAttractiveBeamFX->SetVariableBool(FName(TEXT("User.BeamEnding")), true);
	ActiveAttractiveBeamFX->SetVariableFloat(FName(TEXT("User.BeamFadeOutTime")), BeamFXFadeOutTime);

	if (BeamFXFadeOutTime <= 0.f)
	{
		FinishAttractiveBeamFX();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BeamFXCleanupTimerHandle);
		World->GetTimerManager().SetTimer(
			BeamFXCleanupTimerHandle,
			this,
			&UGA_AttractiveBeam::FinishAttractiveBeamFX,
			BeamFXFadeOutTime,
			false);
		return;
	}

	FinishAttractiveBeamFX();
}

// 매료빔 시작 페이드 완료 후 Niagara에 정상 상태를 알린다.
void UGA_AttractiveBeam::FinishAttractiveBeamFadeIn()
{
	if (ActiveAttractiveBeamFX)
	{
		ActiveAttractiveBeamFX->SetVariableBool(FName(TEXT("User.BeamStarting")), false);
	}
}

//VFX 스케일/페이드 아웃 종료, 이펙트 제거.
void UGA_AttractiveBeam::FinishAttractiveBeamFX()
{
	if (!ActiveAttractiveBeamFX)
	{
		bIsAttractiveBeamFXEnding = false;
		return;
	}

	ActiveAttractiveBeamFX->Deactivate();
	ActiveAttractiveBeamFX->DestroyComponent();
	ActiveAttractiveBeamFX = nullptr;
	bIsAttractiveBeamFXEnding = false;
}

//VFX용 소켓 있으면 위치 확인해서 반환.
FVector UGA_AttractiveBeam::GetAttractiveBeamFXStartLocation() const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return FVector::ZeroVector;
	}

	if (const ACharacter* Character = Cast<ACharacter>(Avatar))
	{
		if (const USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			if (Mesh->DoesSocketExist(AttractiveBeamSocketName))
			{
				return Mesh->GetSocketLocation(AttractiveBeamSocketName);
			}
		}
	}

	return Avatar->GetActorLocation();
}

//GA 사용하는 아바타의 색상 반환
FLinearColor UGA_AttractiveBeam::GetAvatarPlayerColor() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const APawn* Pawn = Cast<APawn>(Avatar);
	const AMTPlayerState* MTPS = Pawn ? Pawn->GetPlayerState<AMTPlayerState>() : nullptr;
	return MTPS ? MTPS->GetTeamColor() : FLinearColor::White;
}

//조준점 산정, 플레이어와 카메라 사이에 있는 장애물은 무시. 추후 GA_HeartBeam 조준과 함께 라이브러리 분리 필요.
bool UGA_AttractiveBeam::TraceBeam(
	FVector& OutTraceStart,
	FVector& OutBeamEnd,
	AMTPedestrianBase*& OutPedestrian,
	bool& bOutHitActor,
	bool& bOutHitWorld) const
{
	OutTraceStart = FVector::ZeroVector;
	OutBeamEnd = FVector::ZeroVector;
	OutPedestrian = nullptr;
	bOutHitActor = false;
	bOutHitWorld = false;

	AActor* Avatar = GetAvatarActorFromActorInfo();
	const APawn* Pawn = Cast<APawn>(Avatar);
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UWorld* World = GetWorld();
	if (!PC || !World)
	{
		return false;
	}

	FVector CamLocation;
	FRotator CamRotation;
	PC->GetPlayerViewPoint(CamLocation, CamRotation);
	const FVector AimDirection = CamRotation.Vector();
	const FVector CameraTraceEnd = CamLocation + (AimDirection * TraceDistance);
	OutTraceStart = GetAttractiveBeamFXStartLocation();

	// 1) 카메라 정면 LineTrace로 시각적 조준점을 계산한다.
	// 월드 Trace에서는 Pawn을 제외하고 벽/지형만 찾는다.
	FCollisionQueryParams CameraWorldParams;
	CameraWorldParams.AddIgnoredActor(Avatar);
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		CameraWorldParams.AddIgnoredActor(*It);
	}

	FVector CameraTarget = CameraTraceEnd;
	FHitResult WallHit;
	if (World->LineTraceSingleByChannel(WallHit, CamLocation, CameraTraceEnd, ECC_Visibility, CameraWorldParams))
	{
		CameraTarget = WallHit.ImpactPoint;
	}

	// Pawn도 반경 없는 LineTrace로 찾는다. 카메라와 플레이어 사이 Hit는 제외한다.
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
		AActor* HitActor = Hit.GetActor();
		if (!IsValid(HitActor))
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

	// 카메라 뒤 장애물은 플레이어 시작 빔을 막지 않는다.
	OutBeamEnd = FVector::DotProduct(CameraTarget - OutTraceStart, AimDirection) > 0.f
		? CameraTarget
		: OutTraceStart + (AimDirection * TraceDistance);

	FHitResult WorldHit;
	bOutHitWorld = World->LineTraceSingleByChannel(
		WorldHit,
		OutTraceStart,
		OutBeamEnd,
		ECC_Visibility,
		CameraWorldParams);
	if (bOutHitWorld)
	{
		OutBeamEnd = WorldHit.ImpactPoint;
	}

	// 3) 데미지 판정만 소켓→시각 끝점 Sphere Sweep으로 수행한다.
	// 판정 Hit로 OutBeamEnd를 덮어쓰지 않는다.
	FCollisionQueryParams DamageParams;
	DamageParams.AddIgnoredActor(Avatar);
	TArray<FHitResult> DamageHits;
	World->SweepMultiByObjectType(
		DamageHits,
		OutTraceStart,
		OutBeamEnd,
		FQuat::Identity,
		PawnObjectParams,
		FCollisionShape::MakeSphere(BeamRadius),
		DamageParams);

	for (const FHitResult& Hit : DamageHits)
	{
		AActor* HitActor = Hit.GetActor();
		if (!IsValid(HitActor))
		{
			continue;
		}
		bOutHitActor = true;
		if (AMTPedestrianBase* Pedestrian = Cast<AMTPedestrianBase>(HitActor))
		{
			OutPedestrian = Pedestrian;
			break;
		}
	}

	return true;
}
