#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Game/MTTypes.h"
#include "MTPlayerController.generated.h"

UCLASS()
class MEOWTRACTIVE_API AMTPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// 로비 버튼 → 서버 요청 (BP UI에서 호출)
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "MT|Lobby")
	void Server_SetSelectedCat(EMTCatType NewCat);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "MT|Lobby")
	void Server_ToggleReady();

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "MT|Lobby")
	void Server_RequestStartMatch();

	UFUNCTION(Client, Reliable)
	void ClientShowResult();

	UPROPERTY(EditDefaultsOnly, Category = "MT|UI")
	TSubclassOf<class UMTMatchGameResultWidget> ResultWidgetClass;

private:
	float LastReadyToggleTime = -100.f;     // 서버 쿨다운 기준 시각
	static constexpr float ReadyCooldown = 0.5f;
};
