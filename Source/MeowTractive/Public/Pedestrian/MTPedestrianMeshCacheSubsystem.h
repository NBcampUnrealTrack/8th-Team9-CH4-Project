#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MTPedestrianMeshCacheSubsystem.generated.h"

class USkeletalMesh;

UCLASS()
class MEOWTRACTIVE_API UMTPedestrianMeshCacheSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	USkeletalMesh* FindMesh(const FString& CacheKey) const;
	void StoreMesh(const FString& CacheKey, USkeletalMesh* Mesh);

private:
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<USkeletalMesh>> CachedMeshes;
};
