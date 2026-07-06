#include "Pedestrian/MTPedestrianMeshCacheSubsystem.h"

#include "Engine/SkeletalMesh.h"

//만들었던 메시 캐싱 되어있으면 반환
USkeletalMesh* UMTPedestrianMeshCacheSubsystem::FindMesh(const FString& CacheKey) const
{
	const TObjectPtr<USkeletalMesh>* Found = CachedMeshes.Find(CacheKey);
	return Found ? Found->Get() : nullptr;
}

//새로운 메시 캐싱
void UMTPedestrianMeshCacheSubsystem::StoreMesh(const FString& CacheKey, USkeletalMesh* Mesh)
{
	if (!CacheKey.IsEmpty() && Mesh)
	{
		CachedMeshes.FindOrAdd(CacheKey) = Mesh;
	}
}
