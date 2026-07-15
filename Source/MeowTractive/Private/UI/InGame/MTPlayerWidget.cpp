#include "UI/InGame/MTPlayerWidget.h"
#include "UI/InGame/MTSkillSlotWidget.h"
#include "Game/MTGameState.h"
#include "Game/MTGameplayTags.h"
#include "Player/MTPlayerCharacter.h"
#include "Player/MTPlayerState.h"
#include "Player/MTPlayerAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/VerticalBox.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "GameFramework/PlayerState.h"

void UMTPlayerWidget::NativeConstruct()
{
	Super::NativeConstruct();
	TryBindGameState();
	TryBindCharacter();
}

void UMTPlayerWidget::NativeDestruct()
{
	if (AMTGameState* GS = BoundGameState.Get())
	{
		GS->OnPlayerScoresUpdated.RemoveDynamic(this, &UMTPlayerWidget::HandleScoresUpdated);
		GS->OnMatchTimeUpdated.RemoveDynamic(this, &UMTPlayerWidget::HandleMatchTimeUpdated);
	}
	UnbindCharacter();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitMarkerHideTimerHandle);
	}

	Super::NativeDestruct();
}

void UMTPlayerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!BoundGameState.IsValid())
	{
		TryBindGameState();
	}
	// 리스폰/폰 교체 감지 → 재바인딩
	if (GetOwningPlayerPawn() != BoundCharacter.Get())
	{
		TryBindCharacter();
	}
}

void UMTPlayerWidget::TryBindGameState()
{
	AMTGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMTGameState>() : nullptr;
	if (!GS)
	{
		return;
	}

	BoundGameState = GS;
	GS->OnPlayerScoresUpdated.AddUniqueDynamic(this, &UMTPlayerWidget::HandleScoresUpdated);
	GS->OnMatchTimeUpdated.AddUniqueDynamic(this, &UMTPlayerWidget::HandleMatchTimeUpdated);

	// 초기값 즉시 반영
	HandleScoresUpdated();
	HandleMatchTimeUpdated(GS->GetMatchRemainingTime());
}

void UMTPlayerWidget::TryBindCharacter()
{
	UnbindCharacter();

	AMTPlayerCharacter* Character = Cast<AMTPlayerCharacter>(GetOwningPlayerPawn());
	UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
	if (!Character || !ASC)
	{
		return;
	}

	BoundCharacter = Character;
	BoundASC = ASC;

	HpChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UMTPlayerAttributeSet::GetHpAttribute())
		.AddUObject(this, &UMTPlayerWidget::HandleHpChanged);
	MaxHpChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UMTPlayerAttributeSet::GetMaxHpAttribute())
		.AddUObject(this, &UMTPlayerWidget::HandleHpChanged);

	// 스킬 슬롯 연결 (스펙 복제 지연은 슬롯이 자체 재시도)
	if (PunchSlot)
	{
		PunchSlot->BindAbilityByTag(ASC, MTGameplayTags::Ability::TAG_Skill_Attack_MeowPunch);
	}
	if (DashSlot)
	{
		DashSlot->BindDash(Character);
	}
	if (SkillASlot)
	{
		SkillASlot->BindAbilityBySlot(ASC, (int32)EMTAbilitySlot::SkillA);
	}
	if (SkillBSlot)
	{
		SkillBSlot->BindAbilityBySlot(ASC, (int32)EMTAbilitySlot::SkillB);
	}

	RefreshHp();
	RefreshAccentColor();
}

void UMTPlayerWidget::UnbindCharacter()
{
	if (UAbilitySystemComponent* ASC = BoundASC.Get())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UMTPlayerAttributeSet::GetHpAttribute()).Remove(HpChangedHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(UMTPlayerAttributeSet::GetMaxHpAttribute()).Remove(MaxHpChangedHandle);
	}
	HpChangedHandle.Reset();
	MaxHpChangedHandle.Reset();
	BoundCharacter = nullptr;
	BoundASC = nullptr;
}

void UMTPlayerWidget::HandleScoresUpdated()
{
	RefreshRanking();
	RefreshAttractedCount();
	RefreshAccentColor();   // TeamColor 복제 지연 대비 — 점수 갱신 때마다 재적용
}

void UMTPlayerWidget::RefreshAccentColor()
{
	const AMTPlayerState* MyPS = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<AMTPlayerState>() : nullptr;
	if (!MyPS)
	{
		return;
	}
	const FLinearColor MyColor = MyPS->GetTeamColor();

	UMTSkillSlotWidget* Slots[] = { PunchSlot, DashSlot, SkillASlot, SkillBSlot };
	for (UMTSkillSlotWidget* SkillSlot : Slots)
	{
		if (SkillSlot)
		{
			SkillSlot->SetAccentColor(MyColor);
		}
	}
}

void UMTPlayerWidget::HandleMatchTimeUpdated(int32 RemainingSeconds)
{
	if (TimeText)
	{
		TimeText->SetText(FText::FromString(
			FString::Printf(TEXT("%d:%02d"), RemainingSeconds / 60, RemainingSeconds % 60)));
	}
}

void UMTPlayerWidget::ShowHitMarker()
{
	if (!HitMarker)
	{
		return;
	}

	HitMarker->SetVisibility(ESlateVisibility::HitTestInvisible);

	// 매 호출마다 숨김 타이머 리셋 → 빔이 계속 닿는 동안은 계속 보임
	GetWorld()->GetTimerManager().SetTimer(
		HitMarkerHideTimerHandle,
		this,
		&UMTPlayerWidget::HideHitMarker,
		0.15f,
		false
	);
}

void UMTPlayerWidget::HideHitMarker()
{
	if (HitMarker)
	{
		HitMarker->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMTPlayerWidget::HandleHpChanged(const FOnAttributeChangeData& Data)
{
	RefreshHp();
}

void UMTPlayerWidget::RefreshRanking()
{
	const AMTGameState* GS = BoundGameState.Get();
	if (!GS)
	{
		return;
	}

	const APlayerState* MyPS = GetOwningPlayer() ? GetOwningPlayer()->PlayerState : nullptr;
	const TArray<FPlayerScore> Sorted = GS->GetSortedPlayerScores();

	auto MakeLine = [&](int32 Index) -> FString
	{
		const APlayerState* PS = Sorted[Index].PlayerState;
		const FString Name = PS ? PS->GetPlayerName() : TEXT("???");
		return FString::Printf(TEXT("%s%d위 %s  %d명"),
			(PS && PS == MyPS) ? TEXT("▶ ") : TEXT("   "),
			Index + 1, *Name, Sorted[Index].AttractedCount);
	};

	// 플레이어별 TeamColor 행 (매치가 슬롯별로 부여한 색)
	if (RankingBox)
	{
		if (RankingText)
		{
			RankingText->SetVisibility(ESlateVisibility::Collapsed);
		}
		RankingBox->ClearChildren();
		for (int32 i = 0; i < Sorted.Num(); ++i)
		{
			UTextBlock* Row = WidgetTree->ConstructWidget<UTextBlock>();
			Row->SetText(FText::FromString(MakeLine(i)));
			if (RankingRowFont.HasValidFont())
			{
				Row->SetFont(RankingRowFont);
			}
			const AMTPlayerState* MTPS = Cast<AMTPlayerState>(Sorted[i].PlayerState.Get());
			Row->SetColorAndOpacity(FSlateColor(MTPS ? MTPS->GetTeamColor() : FLinearColor::White));
			Row->SetShadowOffset(FVector2D(1.5, 1.5));
			Row->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f));
			RankingBox->AddChildToVerticalBox(Row);
		}
		return;
	}

	// 폴백: 단색 멀티라인
	if (!RankingText)
	{
		return;
	}
	FString Lines;
	for (int32 i = 0; i < Sorted.Num(); ++i)
	{
		Lines += MakeLine(i);
		if (i < Sorted.Num() - 1)
		{
			Lines += TEXT("\n");
		}
	}
	RankingText->SetText(FText::FromString(Lines));
}

void UMTPlayerWidget::RefreshAttractedCount()
{
	const AMTGameState* GS = BoundGameState.Get();
	APlayerState* MyPS = GetOwningPlayer() ? GetOwningPlayer()->PlayerState : nullptr;
	if (!GS || !AttractedCountText || !MyPS)
	{
		return;
	}
	AttractedCountText->SetText(FText::AsNumber(GS->GetAttractedCount(MyPS)));
}

void UMTPlayerWidget::RefreshHp()
{
	const UAbilitySystemComponent* ASC = BoundASC.Get();
	if (!ASC)
	{
		return;
	}

	const float Hp = ASC->GetNumericAttribute(UMTPlayerAttributeSet::GetHpAttribute());
	const float MaxHp = ASC->GetNumericAttribute(UMTPlayerAttributeSet::GetMaxHpAttribute());

	if (HpBar)
	{
		HpBar->SetPercent(MaxHp > 0.f ? Hp / MaxHp : 0.f);
	}
	if (HpText)
	{
		HpText->SetText(FText::FromString(
			FString::Printf(TEXT("%.0f / %.0f"), FMath::Max(0.f, Hp), MaxHp)));
	}
}
