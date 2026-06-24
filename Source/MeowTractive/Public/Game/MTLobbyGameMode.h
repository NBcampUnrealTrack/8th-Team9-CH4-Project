#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MTLobbyGameMode.generated.h"

UCLASS()
class MEOWTRACTIVE_API AMTLobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMTLobbyGameMode();

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	// 전원 준비 완료 + 최소 1명
	bool CanStartMatch() const;

	// 호스트 시작 요청 처리 (조건 충족 시 다음 맵으로 ServerTravel)
	void TryStartMatch();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "MT|Lobby")
	int32 MaxPlayers = 4;

	// 매치 맵 (게임시작 시 이동)
	UPROPERTY(EditDefaultsOnly, Category = "MT|Lobby")
	FString NextMapPath = TEXT("/Game/Map/Map_Insa/Prototype_Insadong");

private:
	int32 AcquireSlot();
	void ReleaseSlot(int32 Slot);

	TArray<bool> UsedSlots;     // 슬롯 점유 현황 (크기 = MaxPlayers)
};
