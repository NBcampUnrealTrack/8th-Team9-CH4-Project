#include "UI/MainMenu/MTMenuWidget.h"
#include "Game/MTGameInstance.h"
#include "Game/MTLog.h"
#include "UI/Settings/MTSettingsWidget.h"
#include "CommonButtonBase.h"
#include "Components/Widget.h"
#include "Components/TextBlock.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

// 콘솔: `MT.Local 1` — 로컬 테스트용 LAN 강제 (0이면 MenuSetup 값 사용)
static TAutoConsoleVariable<int32> CVarMTLocal(
	TEXT("MT.Local"),
	0,
	TEXT("세션 생성/검색을 LAN으로 강제 (1=LAN, 0=MenuSetup 값)"),
	ECVF_Default);

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
	if (GameFlow)
	{
		// 접속 진행 상태 → "접속 중" 오버레이 표시 (재진입 시 현재 상태 즉시 반영)
		GameFlow->OnConnectingStateChanged.AddUniqueDynamic(this, &UMTMenuWidget::HandleConnectingStateChanged);
		HandleConnectingStateChanged(GameFlow->IsConnecting());
	}
	MTMenuScreen(GameFlow ? FColor::Green : FColor::Red,
		FString::Printf(TEXT("[MTMenu] MenuSetup 완료 (GameFlow 캐스팅=%d)"), GameFlow ? 1 : 0));
}

void UMTMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Mt_MainSetting)
	{
		Mt_MainSetting->OnClicked().AddUObject(this, &UMTMenuWidget::OpenSettings);
	}
}

void UMTMenuWidget::NativeDestruct()
{
	if (Mt_MainSetting)
	{
		Mt_MainSetting->OnClicked().RemoveAll(this);
	}
	if (GameFlow)
	{
		GameFlow->OnConnectingStateChanged.RemoveDynamic(this, &UMTMenuWidget::HandleConnectingStateChanged);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ConnectingDotsTimer);
	}
	Super::NativeDestruct();
}

void UMTMenuWidget::OpenSettings()
{
	if (!SettingsWidgetClass)
	{
		MTMenuScreen(FColor::Yellow, TEXT("[MTMenu] SettingsWidgetClass 미지정 → 설정 열기 불가"));
		return;
	}
	if (UMTSettingsWidget* Settings = CreateWidget<UMTSettingsWidget>(GetOwningPlayer(), SettingsWidgetClass))
	{
		Settings->AddToViewport(60);
		Settings->SetUserFocus(GetOwningPlayer());   // ESC 닫기 수신
	}
}

void UMTMenuWidget::HandleConnectingStateChanged(bool bConnecting)
{
	if (ConnectingOverlay)
	{
		ConnectingOverlay->SetVisibility(bConnecting ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// 표시 중에만 점 애니메이션 (. → .. → ... 반복)
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (bConnecting && ConnectingText)
	{
		// WBP 원문을 기본 문구로 사용 (끝의 점/말줄임표는 애니메이션이 대신 그림)
		if (ConnectingBaseText.IsEmpty())
		{
			ConnectingBaseText = ConnectingText->GetText().ToString();
			while (ConnectingBaseText.EndsWith(TEXT(".")) || ConnectingBaseText.EndsWith(TEXT("…")))
			{
				ConnectingBaseText.LeftChopInline(1);
			}
		}
		ConnectingDotCount = -1;   // 첫 표시가 점 0개("접속 중")부터 시작
		TickConnectingDots();
		World->GetTimerManager().SetTimer(ConnectingDotsTimer, this, &UMTMenuWidget::TickConnectingDots, 0.4f, true);
	}
	else
	{
		World->GetTimerManager().ClearTimer(ConnectingDotsTimer);
	}
}

void UMTMenuWidget::TickConnectingDots()
{
	if (ConnectingText)
	{
		ConnectingDotCount = (ConnectingDotCount + 1) % 4;   // 0 → 1 → 2 → 3 반복
		ConnectingText->SetText(FText::FromString(ConnectingBaseText + FString::ChrN(ConnectingDotCount, TEXT('.'))));
	}
}

bool UMTMenuWidget::UseLANForSession() const
{
	return bUseLAN || CVarMTLocal.GetValueOnGameThread() != 0;
}

void UMTMenuWidget::HostButtonClicked()
{
	// 빠른 시작: 참가 가능한 공개방 검색 → 있으면 참가, 없으면 기본 설정으로 방 생성
	MTMenuScreen(FColor::Cyan, FString::Printf(TEXT("[MTMenu] QuickStart (GameFlow=%d, LAN=%d)"), GameFlow ? 1 : 0, UseLANForSession() ? 1 : 0));
	if (GameFlow)
	{
		GameFlow->QuickStart(NumConnections, UseLANForSession());
	}
	else
	{
		MTMenuScreen(FColor::Red, TEXT("[MTMenu] GameFlow null → MenuSetup 호출 안 됨! (Event Construct에서 MenuSetup 필요)"));
	}
}

void UMTMenuWidget::HostWithSettings(FMTRoomSettings RoomSettings)
{
	MTMenuScreen(FColor::Cyan, FString::Printf(TEXT("[MTMenu] HostWithSettings '%s' (GameFlow=%d, LAN=%d)"),
		*RoomSettings.RoomName, GameFlow ? 1 : 0, UseLANForSession() ? 1 : 0));
	if (GameFlow)
	{
		GameFlow->HostGame(RoomSettings, NumConnections, UseLANForSession());
	}
	else
	{
		MTMenuScreen(FColor::Red, TEXT("[MTMenu] GameFlow null → MenuSetup 호출 안 됨! (Event Construct에서 MenuSetup 필요)"));
	}
}

void UMTMenuWidget::JoinButtonClicked()
{
	MTMenuScreen(FColor::Cyan, FString::Printf(TEXT("[MTMenu] JoinButtonClicked (GameFlow=%d, LAN=%d)"), GameFlow ? 1 : 0, UseLANForSession() ? 1 : 0));
	if (GameFlow)
	{
		GameFlow->JoinGame(UseLANForSession());
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
