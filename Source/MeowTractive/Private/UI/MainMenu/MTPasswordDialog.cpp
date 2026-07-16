#include "UI/MainMenu/MTPasswordDialog.h"
#include "CommonButtonBase.h"
#include "Components/EditableText.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Animation/WidgetAnimation.h"

void UMTPasswordDialog::NativeConstruct()
{
	Super::NativeConstruct();

	// 버튼 배선은 C++이 담당 (WBP는 배치만)
	if (ConfirmButton)
	{
		ConfirmButton->OnClicked().AddUObject(this, &UMTPasswordDialog::Confirm);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked().AddUObject(this, &UMTPasswordDialog::Cancel);
	}
	// 엔터로 확인 (입력 중 바로 제출)
	if (PasswordInput)
	{
		PasswordInput->OnTextCommitted.AddDynamic(this, &UMTPasswordDialog::HandleTextCommitted);
	}

	if (FadeIn)
	{
		PlayAnimation(FadeIn);
	}
}

void UMTPasswordDialog::NativeDestruct()
{
	if (ConfirmButton)
	{
		ConfirmButton->OnClicked().RemoveAll(this);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked().RemoveAll(this);
	}
	if (PasswordInput)
	{
		PasswordInput->OnTextCommitted.RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UMTPasswordDialog::HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		Confirm();
	}
}

void UMTPasswordDialog::SetupForSearch()
{
	bSearchMode = true;
	CachedRoomName.Empty();

	if (TitleText)
	{
		TitleText->SetText(NSLOCTEXT("MT", "SearchRoomTitle", "방 검색"));
	}
	if (RoomNameRow)
	{
		RoomNameRow->SetVisibility(ESlateVisibility::Visible);
	}
}

void UMTPasswordDialog::SetupForPassword(const FString& InRoomName)
{
	bSearchMode = false;
	CachedRoomName = InRoomName;

	if (TitleText)
	{
		// 어느 방에 들어가는지 제목에 표시
		TitleText->SetText(InRoomName.IsEmpty()
			? NSLOCTEXT("MT", "PasswordTitle", "비밀번호 입력")
			: FText::FromString(FString::Printf(TEXT("%s — 비밀번호 입력"), *InRoomName)));
	}
	if (RoomNameRow)
	{
		RoomNameRow->SetVisibility(ESlateVisibility::Collapsed);   // 방이 이미 정해짐
	}
}

void UMTPasswordDialog::Confirm()
{
	const FString Room = (bSearchMode && RoomNameInput)
		? RoomNameInput->GetText().ToString()
		: CachedRoomName;
	const FString Password = PasswordInput ? PasswordInput->GetText().ToString() : FString();

	OnSubmitted.Broadcast(Room, Password);
	CloseDialog();
}

void UMTPasswordDialog::Cancel()
{
	OnCancelled.Broadcast();
	CloseDialog();
}

void UMTPasswordDialog::CloseDialog()
{
	DeactivateWidget();
	RemoveFromParent();
}

UWidget* UMTPasswordDialog::NativeGetDesiredFocusTarget() const
{
	// 열리자마자 바로 타이핑 (검색은 방이름부터, 비밀번호 모드는 비밀번호칸)
	if (bSearchMode && RoomNameInput)
	{
		return RoomNameInput;
	}
	return PasswordInput ? PasswordInput : Super::NativeGetDesiredFocusTarget();
}

TOptional<FUIInputConfig> UMTPasswordDialog::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}
