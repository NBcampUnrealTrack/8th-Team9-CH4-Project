#include "UI/MainMenu/MTConfirmationDialog.h"
#include "Animation/WidgetAnimation.h"

void UMTConfirmationDialog::NativeConstruct()
{
	Super::NativeConstruct();
	if (FadeIn)
	{
		PlayAnimation(FadeIn);   // 열릴 때 페이드인
	}
}

void UMTConfirmationDialog::Confirm()
{
	OnConfirmed.Broadcast();     // 네: 여는 쪽이 처리(예: QuitGame)
	RemoveFromParent();
}

void UMTConfirmationDialog::Cancel()
{
	OnCancelled.Broadcast();
	RemoveFromParent();          // 아니요: 다이얼로그만 닫기
}
