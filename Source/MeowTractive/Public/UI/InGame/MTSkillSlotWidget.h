#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpecHandle.h"
#include "MTSkillSlotWidget.generated.h"

class UImage;
class UTextBlock;
class UProgressBar;
class UTexture2D;
class UAbilitySystemComponent;
class AMTPlayerCharacter;

/** 스킬 슬롯: 아이콘 + 쿨다운(초/진행률) 표시. 대시 슬롯은 잔여 충전 수 + 재충전 진행 표시. */
UCLASS()
class MEOWTRACTIVE_API UMTSkillSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 고정 스킬(근접공격 등): 어빌리티 태그로 스펙 검색 (스펙 복제 지연 → 틱에서 재시도)
	void BindAbilityByTag(UAbilitySystemComponent* InASC, FGameplayTag InAbilityTag);

	// 제네릭 슬롯(Q/E): 슬롯 InputID로 스펙 검색
	void BindAbilityBySlot(UAbilitySystemComponent* InASC, int32 InInputID);

	// 대시: 잔여 충전 수 + 재충전 진행 (캐릭터 복제값 기준)
	void BindDash(AMTPlayerCharacter* InCharacter);

	// 플레이어 색 강조 (쿨다운 바/카운트) — 매치가 부여한 TeamColor를 HUD가 전달
	void SetAccentColor(const FLinearColor& InColor);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> SkillIcon;

	// 남은 쿨다운 초 (올림). 쿨다운 없으면 숨김
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CooldownText;

	// 남은 쿨다운 비율 (1→0)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> CooldownBar;

	// 대시 잔여 충전 수 (대시 슬롯 전용)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CountText;

	// 어빌리티에 AbilityIcon이 없을 때 사용할 아이콘 (WBP 인스턴스별 지정)
	UPROPERTY(EditAnywhere, Category = "MT|UI")
	TObjectPtr<UTexture2D> DefaultIcon;

private:
	// 스펙 검색 + 어빌리티 아이콘 적용 (스펙 복제 지연/리스폰 대응)
	void ResolveAbilitySpec();
	void UpdateAbilityCooldown();
	void UpdateDashSlot();
	void SetCooldownVisuals(float Remaining, float Duration, bool bDimIcon);

	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	TWeakObjectPtr<AMTPlayerCharacter> DashCharacter;
	FGameplayTag AbilityTag;
	int32 SlotInputID = INDEX_NONE;
	FGameplayAbilitySpecHandle SpecHandle;
	bool bDashSlot = false;
};
