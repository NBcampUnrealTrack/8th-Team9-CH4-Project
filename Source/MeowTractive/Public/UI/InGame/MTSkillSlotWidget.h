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
class UEnhancedInputUserSettings;

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

	// 슬롯 키 매핑 이름 지정 → SkillKeyText에 현재 키 표시 (HUD가 바인딩 시 호출)
	void SetKeyMappingName(FName InMappingName);

	// 키가 없는 슬롯(패시브)에서 SkillKeyText에 대신 표시할 고정 문구
	void SetNoKeyLabel(const FText& InLabel);

	// 정보 패널용: 아이콘만 표시 (쿨다운/카운트 갱신 없이 항상 활성 상태)
	void SetIconOnly(bool bInIconOnly);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
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

	// 슬롯 키 표시 (매핑 이름이 있으면 현재 키를 표시, 없으면 숨김)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SkillKeyText;

	// 이 슬롯의 입력 매핑 이름(FName). 예: IA_Dash / IA_SkillA / IA_SkillB. 비면 키 표시 안 함
	UPROPERTY(EditAnywhere, Category = "MT|UI")
	FName KeyMappingName;

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

	// KeyMappingName에 대응하는 현재 키를 SkillKeyText에 표시 (없으면 숨김)
	void RefreshKeyText();

	// 로컬 플레이어의 Enhanced Input 사용자 설정 (키 조회/변경 구독용)
	UEnhancedInputUserSettings* GetInputUserSettings() const;

	UFUNCTION()
	void HandleInputSettingsChanged(UEnhancedInputUserSettings* Settings);

	// 키 없는 슬롯의 대체 문구 (패시브 = "패시브"). 비면 그냥 숨김
	FText NoKeyLabel;

	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	TWeakObjectPtr<AMTPlayerCharacter> DashCharacter;
	FGameplayTag AbilityTag;
	int32 SlotInputID = INDEX_NONE;
	TSubclassOf<UGameplayAbility> AbilityClass;   // 클래스 매칭용(패시브·대시)
	FGameplayAbilitySpecHandle SpecHandle;
	bool bDashSlot = false;
	bool bIconOnly = false;

	// SkillIcon 브러시에 적용된 쿨다운 머티리얼 인스턴스 (Progress/IconTex 세팅용)
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CooldownMID;
};
