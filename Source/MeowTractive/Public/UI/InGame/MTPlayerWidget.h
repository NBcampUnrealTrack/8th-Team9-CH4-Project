#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "MTPlayerWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UVerticalBox;
class UMTSkillSlotWidget;
class AMTGameState;
class AMTPlayerCharacter;
class UAbilitySystemComponent;
struct FOnAttributeChangeData;

/** 인게임 HUD: 순위/남은시간/체력/매료 수/스킬 쿨다운. 데이터는 GameState + 소유 폰 ASC 구독. */
UCLASS()
class MEOWTRACTIVE_API UMTPlayerWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 좌상단: 순위 — 행을 동적 생성해 플레이어별 TeamColor로 표시 (있으면 RankingText 대신 사용)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> RankingBox;

	// 순위 행 폰트 (WBP 디폴트에서 지정. 미지정 시 엔진 기본)
	UPROPERTY(EditAnywhere, Category = "MT|UI")
	FSlateFontInfo RankingRowFont;

	// 좌상단: 순위 폴백 (RankingBox 없을 때 단색 멀티라인)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RankingText;

	// 중앙 상단: 남은 시간 (M:SS)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TimeText;

	// 중앙 하단: 체력
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HpBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HpText;

	// 우상단: 내가 매료한 행인 수
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AttractedCountText;

	// 우하단: 스킬 슬롯 (근접공격 / 대시 / Q / E)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UMTSkillSlotWidget> PunchSlot;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UMTSkillSlotWidget> DashSlot;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UMTSkillSlotWidget> SkillASlot;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UMTSkillSlotWidget> SkillBSlot;

private:
	// GameState/폰은 위젯 생성 시점에 없을 수 있음 → 틱에서 준비되는 대로 바인딩 (리스폰 재바인딩 포함)
	void TryBindGameState();
	void TryBindCharacter();
	void UnbindCharacter();

	UFUNCTION()
	void HandleScoresUpdated();

	UFUNCTION()
	void HandleMatchTimeUpdated(int32 RemainingSeconds);

	void HandleHpChanged(const FOnAttributeChangeData& Data);
	void RefreshRanking();
	void RefreshAttractedCount();
	void RefreshHp();

	// 매치가 부여한 내 TeamColor를 스킬 슬롯 강조색으로 전달
	void RefreshAccentColor();

	TWeakObjectPtr<AMTGameState> BoundGameState;
	TWeakObjectPtr<AMTPlayerCharacter> BoundCharacter;
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	FDelegateHandle HpChangedHandle;
	FDelegateHandle MaxHpChangedHandle;
};
