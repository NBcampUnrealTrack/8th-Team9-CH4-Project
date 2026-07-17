#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MTSelectPromptWidget.generated.h"

class UTextBlock;

/** 로비 펀치 선택 프롬프트 — "[현재 근접공격 키] 행동이름" 표시.
 *  키는 EnhancedInput 유저 설정에서 조회 (리바인드 즉시 반영), 행동이름은 소유 액터가 지정. */
UCLASS()
class MEOWTRACTIVE_API UMTSelectPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 프롬프트 뒷부분 문구 ("고양이 선택" / "방 설정" 등) — 소유 액터가 호출
	UFUNCTION(BlueprintCallable, Category = "MT|UI")
	void SetActionLabel(const FText& InLabel);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PromptText;

	// 표시할 키의 매핑 이름 (IA의 PlayerMappableKeySettings Name)
	UPROPERTY(EditDefaultsOnly, Category = "MT|UI")
	FName KeyMappingName = TEXT("IA_MeowPunch");

	// 행동 이름 (WBP 기본값 — 보통 액터가 SetActionLabel로 덮음)
	UPROPERTY(EditAnywhere, Category = "MT|UI")
	FText ActionLabel = NSLOCTEXT("MT", "PromptSelect", "선택");

	// 유저 설정 조회 실패 시 표기할 폴백 키
	UPROPERTY(EditDefaultsOnly, Category = "MT|UI")
	FText FallbackKeyText = NSLOCTEXT("MT", "PromptFallbackKey", "C");

private:
	// "[키] 행동" 갱신
	void RefreshPrompt();

	// 현재 바인딩된 키 표기 (유저 설정 → 폴백 순)
	FText ResolveKeyText() const;

	FTimerHandle RefreshTimer;   // 설정 창에서 리바인드해도 반영되게 주기 갱신
};
