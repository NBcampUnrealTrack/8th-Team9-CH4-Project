#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MTPasswordDialog.generated.h"

class UCommonButtonBase;
class UEditableText;
class UTextBlock;
class UWidget;
class UWidgetAnimation;

// 확인 시 입력값 전달 (RoomName은 검색 모드에서만 채워짐)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMTOnJoinDialogSubmitted, const FString&, RoomName, const FString&, Password);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMTOnJoinDialogCancelled);

/** 참가 입력 다이얼로그 — 검색(방이름+비밀번호) / 비밀번호(선택한 방)의 두 모드.
 *  입력 수집·전달만 담당하고, 실제 참가는 여는 쪽(UMTMenuWidget)이 Flow에 위임. */
UCLASS()
class MEOWTRACTIVE_API UMTPasswordDialog : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// 검색 모드: 방이름 + 비밀번호 입력
	void SetupForSearch();

	// 비밀번호 모드: 선택한 방의 비밀번호만 입력 (방이름 줄은 숨김)
	void SetupForPassword(const FString& InRoomName);

	UFUNCTION(BlueprintCallable, Category = "MT|Dialog")
	void Confirm();

	UFUNCTION(BlueprintCallable, Category = "MT|Dialog")
	void Cancel();

	UPROPERTY(BlueprintAssignable, Category = "MT|Dialog")
	FMTOnJoinDialogSubmitted OnSubmitted;

	UPROPERTY(BlueprintAssignable, Category = "MT|Dialog")
	FMTOnJoinDialogCancelled OnCancelled;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	// 제목 ("방 검색" / "비밀번호 입력")
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	// 방이름 줄(라벨+입력 묶음) — 비밀번호 모드에선 숨김
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> RoomNameRow;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEditableText> RoomNameInput;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableText> PasswordInput;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> ConfirmButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> CancelButton;

	UPROPERTY(meta = (BindWidgetAnimOptional), Transient)
	TObjectPtr<UWidgetAnimation> FadeIn;

private:
	// 엔터 입력 시 확인 처리
	UFUNCTION()
	void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	// 자기 파괴 (확인/취소 공통)
	void CloseDialog();

	bool bSearchMode = false;
	FString CachedRoomName;   // 비밀번호 모드에서 대상 방 이름
};
