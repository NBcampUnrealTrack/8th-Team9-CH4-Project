#include "UI/InGame/MTPlayerHUD.h"
#include "UI/InGame/MTPlayerWidget.h"

void AMTPlayerHUD::BeginPlay()
{
	Super::BeginPlay();

	if (PlayerWidgetClass)
	{
		PlayerWidget = CreateWidget<UMTPlayerWidget>(GetOwningPlayerController(), PlayerWidgetClass);
		if (PlayerWidget)
		{
			PlayerWidget->AddToViewport();
		}
	}
}
