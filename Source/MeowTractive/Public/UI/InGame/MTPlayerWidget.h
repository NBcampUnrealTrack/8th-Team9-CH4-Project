#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "MTPlayerWidget.generated.h"

class UTextBlock;
class UCommonTextBlock;
class UProgressBar;
class UVerticalBox;
class UImage;
class UMTSkillSlotWidget;
class UMTSkillDescData;
class AMTGameState;
class AMTPlayerCharacter;
class UAbilitySystemComponent;
struct FOnAttributeChangeData;

/** 인게임 HUD: 순위/남은시간/체력/매료 수/스킬 쿨다운. 데이터는 GameState + 소유 폰 ASC 구독. */
UCLASS()
class MEOWTRACTIVE_API UMTPlayerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 매료빔이 행인에 닿을 때마다 호출 — 지속 피격 중엔 유지, 끊기면 자동 숨김
	UFUNCTION(BlueprintCallable)
	void ShowHitMarker();

	// F1 토글: 스킬 정보 패널 열기/닫기 (열 때 현재 고양이 기준으로 설명/아이콘 갱신)
	UFUNCTION(BlueprintCallable, Category = "MT|HUD")
	void ToggleSkillInfo();

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

	// 순위 행 템플릿 — RankingBox 안에 Collapsed로 배치. 행마다 이걸 복제해 텍스트/팀색만 교체
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> RankingRowTemplate;

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

	// 우하단: 스킬 슬롯 (패시브 / 대시 / Q / E)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UMTSkillSlotWidget> PassiveSlot;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UMTSkillSlotWidget> DashSlot;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UMTSkillSlotWidget> SkillASlot;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UMTSkillSlotWidget> SkillBSlot;

	// F1로 여닫는 스킬 정보 패널 (기본 숨김)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SkillInfoCanvas;

	// 패시브 / 스킬1 / 스킬2 설명 텍스트
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SkillInfo1;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SkillInfo2;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SkillInfo3;

	// 스킬 정보 패널 아이콘 슬롯 (현재 고양이 기준으로 아이콘 표시)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UMTSkillSlotWidget> PassiveSlot_SkillInfo;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UMTSkillSlotWidget> SkillASlot_SkillInfo;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UMTSkillSlotWidget> SkillBSlot_SkillInfo;

	// 고양이별 스킬 설명 데이터 (Class Defaults에서 할당 — 에디터에서 텍스트 편집)
	UPROPERTY(EditAnywhere, Category = "MT|SkillInfo")
	TObjectPtr<UMTSkillDescData> SkillDescData;

	// 중앙: 히트마커 (매료빔 피격 시 표시, 기본 Collapsed)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> HitMarker;

private:
	// GameState/폰은 위젯 생성 시점에 없을 수 있음 → 틱에서 준비되는 대로 바인딩 (리스폰 재바인딩 포함)
	void TryBindGameState();
	void TryBindCharacter();
	void UnbindCharacter();

	UFUNCTION()
	void HandleScoresUpdated();

	UFUNCTION()
	void HandleMatchTimeUpdated(int32 RemainingSeconds);

	UFUNCTION()
	void HideHitMarker();

	void HandleHpChanged(const FOnAttributeChangeData& Data);
	void RefreshRanking();
	void RefreshAttractedCount();
	void RefreshHp();
	void RefreshAccentColor();

	// 현재 고양이의 패시브/스킬 설명을 SkillInfo 텍스트에 반영
	void RefreshSkillInfo();


	TWeakObjectPtr<AMTGameState> BoundGameState;
	TWeakObjectPtr<AMTPlayerCharacter> BoundCharacter;
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	FDelegateHandle HpChangedHandle;
	FDelegateHandle MaxHpChangedHandle;
	FTimerHandle HitMarkerHideTimerHandle;

};
