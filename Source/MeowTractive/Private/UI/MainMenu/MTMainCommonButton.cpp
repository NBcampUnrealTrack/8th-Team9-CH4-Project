#include "UI/MainMenu/MTMainCommonButton.h"
#include "Components/TextBlock.h"

void UMTMainCommonButton::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (Label)
	{
		Label->SetText(ButtonText);   // 인스펙터의 ButtonText를 라벨에 반영
	}
}
