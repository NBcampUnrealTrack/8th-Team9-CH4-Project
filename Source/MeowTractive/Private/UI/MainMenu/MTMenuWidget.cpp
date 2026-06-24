#include "UI/MainMenu/MTMenuWidget.h"
#include "Game/MTGameInstance.h"
#include "Game/MTLog.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"

// 로그 + 화면 동시 출력 (MT.Log 토글 검사)
static void MTMenuScreen(const FColor& Color, const FString& Msg)
{
	if (!MTLogEnabled())
	{
		return;
	}
	UE_LOG(LogMT, Log, TEXT("%s"), *Msg);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 6.f, Color, Msg);
	}
}

void UMTMenuWidget::MenuSetup(int32 NumPublicConnections, bool bIsLAN)
{
	NumConnections = NumPublicConnections;
	bUseLAN = bIsLAN;

	// 표현 셋업
	AddToViewport();
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
	ActivateWidget();   // CommonUI가 GetDesiredInputConfig로 입력 모드/커서 관리

	GameFlow = Cast<UMTGameInstance>(GetGameInstance());
	MTMenuScreen(GameFlow ? FColor::Green : FColor::Red,
		FString::Printf(TEXT("[MTMenu] MenuSetup 완료 (GameFlow 캐스팅=%d)"), GameFlow ? 1 : 0));
}

void UMTMenuWidget::HostButtonClicked()
{
	MTMenuScreen(FColor::Cyan, FString::Printf(TEXT("[MTMenu] HostButtonClicked (GameFlow=%d)"), GameFlow ? 1 : 0));
	if (GameFlow)
	{
		GameFlow->HostGame(NumConnections, bUseLAN);
	}
	else
	{
		MTMenuScreen(FColor::Red, TEXT("[MTMenu] GameFlow null → MenuSetup 호출 안 됨! (Event Construct에서 MenuSetup 필요)"));
	}
}

void UMTMenuWidget::JoinButtonClicked()
{
	MTMenuScreen(FColor::Cyan, FString::Printf(TEXT("[MTMenu] JoinButtonClicked (GameFlow=%d)"), GameFlow ? 1 : 0));
	if (GameFlow)
	{
		GameFlow->JoinGame(bUseLAN);
	}
	else
	{
		MTMenuScreen(FColor::Red, TEXT("[MTMenu] GameFlow null → MenuSetup 호출 안 됨! (Event Construct에서 MenuSetup 필요)"));
	}
}

TOptional<FUIInputConfig> UMTMenuWidget::GetDesiredInputConfig() const
{
	// 메뉴: 게임 입력 정지, UI 내비/커서는 CommonUI가 관리
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}
