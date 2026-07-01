#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "MTGameplayEventNotify.generated.h"

/** 몽타주 타임라인에서 소유 액터(ASC)에게 GameplayEvent를 보내는 범용 노티파이.
 *  어빌리티의 WaitGameplayEvent(EventTag)와 매칭돼 데미지 창 등을 연다. */
UCLASS(meta = (DisplayName = "MT Gameplay Event"))
class MEOWTRACTIVE_API UMTGameplayEventNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

	// 소유 액터에게 보낼 이벤트 태그
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MT")
	FGameplayTag EventTag;
};
