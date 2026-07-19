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
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UAbilitySystemComponent;
class UGameplayAbility;
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

	// 패시브: 어빌리티 클래스로 스펙 검색 (입력·쿨다운 없음 → 정적 아이콘)
	void BindAbilityByClass(UAbilitySystemComponent* InASC, TSubclassOf<UGameplayAbility> InAbilityClass);

	// 대시: 잔여 충전 수 + 재충전 진행 (캐릭터 복제값 기준). 아이콘은 대시 어빌리티에서 해석.
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

	// 방사형 쿨다운 머티리얼 (SkillIcon 브러시로 사용). 미지정 시 아이콘 텍스처를 직접 표시(폴백)
	UPROPERTY(EditAnywhere, Category = "MT|UI")
	TObjectPtr<UMaterialInterface> CooldownMaterial;

private:
	// 스펙 검색 + 어빌리티 아이콘 적용 (스펙 복제 지연/리스폰 대응)
	void ResolveAbilitySpec();
	void UpdateAbilityCooldown();
	void UpdateDashSlot();
	void SetCooldownVisuals(float Remaining, float Duration, bool bDimIcon);

	// 아이콘 적용 — 쿨다운 머티리얼 있으면 MID의 IconTex 파라미터로, 없으면 브러시에 직접
	void ApplyIcon(UTexture2D* IconTexture);

	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	TWeakObjectPtr<AMTPlayerCharacter> DashCharacter;
	FGameplayTag AbilityTag;
	int32 SlotInputID = INDEX_NONE;
	TSubclassOf<UGameplayAbility> AbilityClass;   // 클래스 매칭용(패시브·대시)
	FGameplayAbilitySpecHandle SpecHandle;
	bool bDashSlot = false;

	// SkillIcon 브러시에 적용된 쿨다운 머티리얼 인스턴스 (Progress/IconTex 세팅용)
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CooldownMID;
};
