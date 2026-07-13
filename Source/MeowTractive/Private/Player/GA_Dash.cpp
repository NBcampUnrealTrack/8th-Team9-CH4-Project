#include "Player/GA_Dash.h"

#include "Player/MTPlayerCharacter.h"
#include "Player/MTPlayerAttributeSet.h"
#include "Game/MTLog.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Character.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

UGA_Dash::UGA_Dash()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_Dash::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
								const FGameplayAbilityActorInfo* ActorInfo,
								const FGameplayAbilityActivationInfo ActivationInfo,
								const FGameplayEventData* TriggerEventData)
{
	// 진단: BP 그래프 개입·비용 중복 추적용 스냅샷
	UAbilitySystemComponent* LogASC = GetAbilitySystemComponentFromActorInfo();
	const FGameplayAttribute ChargeAttr = UMTPlayerAttributeSet::GetDashChargesAttribute();
	const float ChargesAtEntry = LogASC ? LogASC->GetNumericAttribute(ChargeAttr) : -1.f;
	if (MTLogEnabled())
	{
		UE_LOG(LogMT, Log, TEXT("[Dash] Activate 진입: 충전=%.0f BP오버라이드=%d Auth=%d"),
			ChargesAtEntry, bHasBlueprintActivate ? 1 : 0, HasAuthority(&ActivationInfo) ? 1 : 0);
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData); // BP 그래프 실행 지점

	const float ChargesAfterBP = LogASC ? LogASC->GetNumericAttribute(ChargeAttr) : -1.f;
	if (MTLogEnabled() && !FMath::IsNearlyEqual(ChargesAtEntry, ChargesAfterBP))
	{
		UE_LOG(LogMT, Warning, TEXT("[Dash] BP 그래프가 충전 변경: %.0f → %.0f (BP측 CommitAbility 중복 의심)"),
			ChargesAtEntry, ChargesAfterBP);
	}

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 충전 소비: Cost GE(DashCharges -1) 커밋. 충전 부족 시 CanActivate에서 이미 차단됨.
	const bool bCommitted = CommitAbility(Handle, ActorInfo, ActivationInfo);
	const float ChargesAfterCommit = LogASC ? LogASC->GetNumericAttribute(ChargeAttr) : -1.f;
	if (MTLogEnabled())
	{
		UE_LOG(LogMT, Log, TEXT("[Dash] C++ Commit %s: 충전 %.0f → %.0f"),
			bCommitted ? TEXT("성공") : TEXT("실패"), ChargesAfterBP, ChargesAfterCommit);
	}
	if (!bCommitted)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 디버그: 대시 직후 남은 충전 (예측 클라·서버 모두)
	if (bDrawDebug && GEngine && LogASC)
	{
		const float Max = LogASC->GetNumericAttribute(UMTPlayerAttributeSet::GetMaxDashChargesAttribute());
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan,
			FString::Printf(TEXT("[Dash] 대시! 남은 충전 %.0f/%.0f"), ChargesAfterCommit, Max));
	}

	const FVector Direction = Character->GetActorForwardVector();

	UAbilityTask_ApplyRootMotionConstantForce* ApplyRootMotionConstantForce =
		UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
			this,
			NAME_None,
			Direction,
			DashDistance / DashDuration,					 // 거리/시간 역산 (cm/s)
			DashDuration,
			false,                                           // bIsAdditive
			nullptr,                                         // StrengthOverTime
			ERootMotionFinishVelocityMode::ClampVelocity,
			FVector::ZeroVector,
			0.f,
			bEnableGravity
		);

	ApplyRootMotionConstantForce->OnFinish.AddDynamic(this, &UGA_Dash::OnDashFinished);
	ApplyRootMotionConstantForce->ReadyForActivation();

	// 돌진 충돌 판정 — 서버에서 대시 동안 주기적으로 검사
	if (HasAuthority(&ActivationInfo) && (DamageEffect || StunEffect))
	{
		HitActors.Reset();
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				HitTimerHandle, this, &UGA_Dash::CheckDashHit, 0.02f, /*bLoop=*/true);
		}
		CheckDashHit(); // 첫 프레임 즉시
	}

	// 충전 소비 후 재충전 시작 (서버 권위). 이미 진행 중이면 리셋하지 않음(순차).
	if (HasAuthority(&ActivationInfo))
	{
		// 타이머는 EndAbility 이후 발동되므로 지금(ActorInfo 유효할 때) ASC 캐시
		CachedASC = GetAbilitySystemComponentFromActorInfo();
		EnsureRechargeTimerRunning();
	}

	//대쉬 이펙트 재생
	FGameplayCueParameters Params;
	Params.Location = Character->GetActorLocation();
	Params.Normal = Direction;
	Params.Instigator = Character;
	Params.EffectCauser = Character;

	LogASC->AddGameplayCue(
		FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Cat.Dash")),
		Params);
}

void UGA_Dash::OnDashFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Dash::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (MTLogEnabled())
	{
		UE_LOG(LogMT, Log, TEXT("[Dash] EndAbility (취소=%d)"), bWasCancelled ? 1 : 0);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitTimerHandle);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	//대쉬 이펙트 제거
	GetAbilitySystemComponentFromActorInfo()->RemoveGameplayCue(
		FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Cat.Dash")));
}

void UGA_Dash::CheckDashHit()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return;
	}

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Avatar);
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);

	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(
		Overlaps, Avatar->GetActorLocation(), FQuat::Identity, ObjParams, FCollisionShape::MakeSphere(HitRadius), Params);

	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), Avatar->GetActorLocation(), HitRadius, 12, FColor::Cyan, false, 0.1f);
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC)
	{
		return;
	}

	const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"));
	const FGameplayTag DurationTag = FGameplayTag::RequestGameplayTag(FName("Data.Duration"));

	for (const FOverlapResult& O : Overlaps)
	{
		AActor* Target = O.GetActor();
		if (!Target || HitActors.Contains(Target))
		{
			continue;
		}
		if (!AMTPlayerCharacter::IsEnemyCat(Avatar, Target))
		{
			continue;
		}
		HitActors.Add(Target);

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
		if (!TargetASC)
		{
			continue;
		}

		// 데미지
		if (DamageEffect)
		{
			FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
			Ctx.AddSourceObject(Avatar);
			FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(DamageEffect, GetAbilityLevel(), Ctx);
			if (Spec.IsValid())
			{
				Spec.Data->SetSetByCallerMagnitude(DamageTag, HitDamage);
				SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
			}
		}

		// 스턴 (지속시간 SetByCaller 주입)
		if (StunEffect)
		{
			FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
			Ctx.AddSourceObject(Avatar);
			FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(StunEffect, GetAbilityLevel(), Ctx);
			if (Spec.IsValid())
			{
				Spec.Data->SetSetByCallerMagnitude(DurationTag, StunDuration);
				SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
			}
		}
	}
}

void UGA_Dash::EnsureRechargeTimerRunning()
{
	UAbilitySystemComponent* ASC = CachedASC.Get();
	UWorld* World = ASC ? ASC->GetWorld() : nullptr;
	if (!ASC || !World)
	{
		if (MTLogEnabled())
		{
			UE_LOG(LogMT, Warning, TEXT("[Dash] 재충전 시작 불가: ASC=%d World=%d"), ASC ? 1 : 0, World ? 1 : 0);
		}
		return;
	}

	// 이미 재충전 진행 중이면 유지 (순차 충전: 새 대시로 타이머를 리셋하지 않음)
	if (World->GetTimerManager().IsTimerActive(RechargeTimerHandle))
	{
		if (MTLogEnabled())
		{
			UE_LOG(LogMT, Log, TEXT("[Dash] 재충전 타이머 이미 가동 중 (남은 %.1f초)"),
				World->GetTimerManager().GetTimerRemaining(RechargeTimerHandle));
		}
		return;
	}

	const float Cur = ASC->GetNumericAttribute(UMTPlayerAttributeSet::GetDashChargesAttribute());
	const float Max = ASC->GetNumericAttribute(UMTPlayerAttributeSet::GetMaxDashChargesAttribute());
	if (Cur >= Max)
	{
		if (MTLogEnabled())
		{
			UE_LOG(LogMT, Log, TEXT("[Dash] 재충전 불필요: %.0f/%.0f"), Cur, Max);
		}
		return;
	}

	ArmRechargeTimer(ASC, World);
	if (MTLogEnabled())
	{
		UE_LOG(LogMT, Log, TEXT("[Dash] 재충전 타이머 시작: %.1f초 후 +1 (현재 %.0f/%.0f)"), RechargeInterval, Cur, Max);
	}

	if (bDrawDebug && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Silver,
			FString::Printf(TEXT("[Dash] 재충전 시작: %.0f초 후 +1 (현재 %.0f/%.0f)"), RechargeInterval, Cur, Max));
	}
}

void UGA_Dash::OnRechargeTick()
{
	UAbilitySystemComponent* ASC = CachedASC.Get();
	UWorld* World = ASC ? ASC->GetWorld() : nullptr;
	if (!ASC || !World)
	{
		return;
	}

	const float Cur = ASC->GetNumericAttribute(UMTPlayerAttributeSet::GetDashChargesAttribute());
	const float Max = ASC->GetNumericAttribute(UMTPlayerAttributeSet::GetMaxDashChargesAttribute());
	const float NewVal = FMath::Min(Cur + 1.f, Max);

	// 서버 권위 재충전 — 소유 클라로 복제됨 (예측 불필요)
	const FGameplayAttribute Attr = UMTPlayerAttributeSet::GetDashChargesAttribute();
	ASC->SetNumericAttributeBase(Attr, NewVal);

	// 진단: set한 값 vs 실제 base/current. base>current면 지속 모디파이어(=Cost GE가 Instant 아님)
	if (MTLogEnabled() || bDrawDebug)
	{
		const float BaseAfter = ASC->GetNumericAttributeBase(Attr);
		const float CurAfter = ASC->GetNumericAttribute(Attr);
		const bool bOk = FMath::IsNearlyEqual(CurAfter, NewVal);
		const FString Msg = FString::Printf(TEXT("[Dash] 충전 set=%.0f → base=%.0f cur=%.0f (Max=%.0f)"),
			NewVal, BaseAfter, CurAfter, Max);
		if (MTLogEnabled())
		{
			UE_LOG(LogMT, Log, TEXT("%s"), *Msg);
		}
		if (bDrawDebug && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 4.f, bOk ? FColor::Green : FColor::Red, Msg);
		}
	}

	// 아직 최대 미만이면 다음 충전 재예약 (순차)
	if (NewVal < Max)
	{
		ArmRechargeTimer(ASC, World);
	}
	else if (AMTPlayerCharacter* Cat = Cast<AMTPlayerCharacter>(ASC->GetAvatarActor()))
	{
		// 완충 → HUD 재충전 표시 종료
		Cat->SetDashRechargeState(0.f, 0.f);
	}
}

void UGA_Dash::ArmRechargeTimer(UAbilitySystemComponent* ASC, UWorld* World)
{
	// EndAbility가 ClearAllTimersForObject(this)로 어빌리티 타이머를 전부 지우므로
	// 델리게이트를 ASC에 바인딩 (GA 인스턴스는 약참조로 안전 호출)
	FTimerDelegate Delegate = FTimerDelegate::CreateWeakLambda(ASC,
		[WeakThis = TWeakObjectPtr<UGA_Dash>(this)]()
		{
			if (UGA_Dash* Self = WeakThis.Get())
			{
				Self->OnRechargeTick();
			}
		});
	World->GetTimerManager().SetTimer(RechargeTimerHandle, Delegate, RechargeInterval, /*bLoop=*/false);

	// HUD 재충전 표시용 — 완료 서버시각 기록 (소유 클라로 복제)
	if (AMTPlayerCharacter* Cat = Cast<AMTPlayerCharacter>(ASC->GetAvatarActor()))
	{
		if (const AGameStateBase* GS = World->GetGameState())
		{
			Cat->SetDashRechargeState(GS->GetServerWorldTimeSeconds() + RechargeInterval, RechargeInterval);
		}
	}
}
