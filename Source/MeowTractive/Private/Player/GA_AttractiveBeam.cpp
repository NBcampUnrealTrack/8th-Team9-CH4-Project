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
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

UGA_AttractiveBeam::UGA_AttractiveBeam()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// 클라 예측 + 서버 실행: 빔/디버그는 즉시, 실제 매료 적용은 서버에서.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	SetAssetTags(FGameplayTagContainer(MTGameplayTags::Ability::TAG_Skill_Attract_Beam));

	// 기본공격: State.Casting 미부여(스킬을 막지 않음). 스킬 시전 중엔 발동만 차단 + 스킬이 이 어빌리티를 취소.
	ActivationBlockedTags.AddTag(MTGameplayTags::State::TAG_State_Casting);
	AttractiveBeamFX = TSoftObjectPtr<UNiagaraSystem>(
		FSoftObjectPath(TEXT("/Game/Niagara/NS_AttractiveBeam.NS_AttractiveBeam")));
}

//빔 발사
void UGA_AttractiveBeam::FireBeam()
{
	FVector TraceStart;
	FVector BeamEnd;
	AMTPedestrianBase* Ped = nullptr;
	bool bTraceHit = TraceBeam(TraceStart, BeamEnd, Ped);
	UpdateAttractiveBeamHitFX(bTraceHit);
	if (!bTraceHit)
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

	UpdateAttractiveBeamFX(GetAttractiveBeamFXStartLocation(), BeamEnd);
	OnBeamUpdate(TraceStart, BeamEnd, bHitPed);
}

//이펙트만 업데이트
void UGA_AttractiveBeam::UpdateBeamVisual()
{
	FVector TraceStart;
	FVector BeamEnd;
	AMTPedestrianBase* Ped = nullptr;
	if (TraceBeam(TraceStart, BeamEnd, Ped))
	{
		const bool bHitPed = (Ped != nullptr);
		UpdateAttractiveBeamFX(GetAttractiveBeamFXStartLocation(), BeamEnd);
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
		}

		bIsAttractiveBeamFXEnding = false;
		ActiveAttractiveBeamFX->SetVariableBool(FName(TEXT("User.BeamEnding")), false);
		ActiveAttractiveBeamFX->SetVariableFloat(FName(TEXT("User.BeamFadeOutTime")), BeamFXFadeOutTime);
		ActiveAttractiveBeamFX->Activate(true);
		ActiveAttractiveBeamFX->SetVariableLinearColor(FName(TEXT("User.PlayerColor")), GetAvatarPlayerColor());
		return;
	}

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

	UNiagaraSystem* AttractiveBeamFXAsset = AttractiveBeamFX.LoadSynchronous();
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

	if (ActiveAttractiveBeamFX)
	{
		bIsAttractiveBeamFXEnding = false;
		ActiveAttractiveBeamFX->SetVariableBool(FName(TEXT("User.BeamEnding")), false);
		ActiveAttractiveBeamFX->SetVariableFloat(FName(TEXT("User.BeamFadeOutTime")), BeamFXFadeOutTime);
		ActiveAttractiveBeamFX->SetVariableLinearColor(FName(TEXT("User.PlayerColor")), GetAvatarPlayerColor());
	}
}

//이펙트 관련 변수 업데이트
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

//빔 적중 여부만 업데이트
void UGA_AttractiveBeam::UpdateAttractiveBeamHitFX(const bool TraceHit)
{
	if (!ActiveAttractiveBeamFX)
	{
		return;
	}

	ActiveAttractiveBeamFX->SetVariableBool(FName(TEXT("User.TraceHit")), TraceHit);
}



void UGA_AttractiveBeam::StopAttractiveBeamFX()
{
	if (!ActiveAttractiveBeamFX || bIsAttractiveBeamFXEnding)
	{
		return;
	}

	bIsAttractiveBeamFXEnding = true;
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

FLinearColor UGA_AttractiveBeam::GetAvatarPlayerColor() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const APawn* Pawn = Cast<APawn>(Avatar);
	const AMTPlayerState* MTPS = Pawn ? Pawn->GetPlayerState<AMTPlayerState>() : nullptr;
	return MTPS ? MTPS->GetTeamColor() : FLinearColor::White;
}

bool UGA_AttractiveBeam::TraceBeam(FVector& OutTraceStart, FVector& OutBeamEnd, AMTPedestrianBase*& OutPedestrian) const
{
	OutTraceStart = FVector::ZeroVector;
	OutBeamEnd = FVector::ZeroVector;
	OutPedestrian = nullptr;

	AActor* Avatar = GetAvatarActorFromActorInfo();
	const APawn* Pawn = Cast<APawn>(Avatar);
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC || !GetWorld())
	{
		return false;
	}

	FVector CamLocation;
	FRotator CamRotation;
	PC->GetPlayerViewPoint(CamLocation, CamRotation);
	OutTraceStart = CamLocation;
	const FVector TraceEnd = OutTraceStart + (CamRotation.Vector() * TraceDistance);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Avatar);

	OutBeamEnd = TraceEnd;
	FHitResult WallHit;
	if (GetWorld()->LineTraceSingleByChannel(WallHit, OutTraceStart, TraceEnd, ECC_Visibility, Params))
	{
		OutBeamEnd = WallHit.ImpactPoint;
	}

	FHitResult PawnHit;
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);
	const bool bPawnHit = GetWorld()->SweepSingleByObjectType(
		PawnHit, OutTraceStart, OutBeamEnd, FQuat::Identity, ObjParams, FCollisionShape::MakeSphere(BeamRadius), Params);

	if (bPawnHit)
	{
		OutBeamEnd = PawnHit.ImpactPoint;
		OutPedestrian = Cast<AMTPedestrianBase>(PawnHit.GetActor());
	}

	return true;
}
