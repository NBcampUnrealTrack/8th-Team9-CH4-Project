#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Pedestrian/MTPedestrianAppearanceTypes.h"
#include "MTPedestrianSpawnManager.generated.h"

//행인 외형 랜덤생성 클래스
//MTMatchGameMode에 등록된 캐릭터 외형 조합하여 랜덤생성
class AMTPedestrianBase;

UCLASS()
class MEOWTRACTIVE_API UMTPedestrianSpawnManager : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Pedestrian", meta = (WorldContext = "WorldContextObject"))
	static AMTPedestrianBase* SpawnPedestrian(
		UObject* WorldContextObject,
		TSubclassOf<AMTPedestrianBase> PedestrianClass,
		const FTransform& SpawnTransform,
		const FMTPedestrianGenerationConfig& Config,
		int32 RandomSeed = -1);

	UFUNCTION(BlueprintCallable, Category = "Pedestrian")
	static bool ApplyAppearance(AMTPedestrianBase* Pedestrian, const FMTPedestrianAppearance& Appearance);

	static FMTPedestrianGenerationConfig MakeTestConfiguration();

private:
	static bool SelectAppearance(
		const FMTPedestrianGenerationConfig& Config,
		FRandomStream& RandomStream,
		FMTPedestrianAppearance& OutAppearance);
};
