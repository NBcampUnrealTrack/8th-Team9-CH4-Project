#include "Pedestrian/MTPedestrianSpawnManager.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Pedestrian/MTPedestrianBase.h"
#include "Pedestrian/MTPedestrianMeshCacheSubsystem.h"
#include "SkeletalMergingLibrary.h"

namespace
{
template <typename TObjectType>
TObjectType* LoadRandomAsset(const TArray<TSoftObjectPtr<TObjectType>>& Assets, FRandomStream& RandomStream)
{
	if (Assets.IsEmpty())
	{
		return nullptr;
	}

	const int32 StartIndex = RandomStream.RandRange(0, Assets.Num() - 1);
	for (int32 Offset = 0; Offset < Assets.Num(); ++Offset)
	{
		const int32 Index = (StartIndex + Offset) % Assets.Num();
		if (TObjectType* Asset = Assets[Index].LoadSynchronous())
		{
			return Asset;
		}
	}
	return nullptr;
}

UMaterialInterface* SelectReplacementMaterial(
	const FString& SlotDescription,
	const FMTPedestrianAppearance& Appearance)
{
	if (SlotDescription.Contains(TEXT("hair")))
	{
		return Appearance.HairMaterial;
	}
	if (SlotDescription.Contains(TEXT("skin")) || SlotDescription.Contains(TEXT("head")))
	{
		return Appearance.SkinMaterial;
	}
	if (SlotDescription.Contains(TEXT("upper")) || SlotDescription.Contains(TEXT("torso")))
	{
		return Appearance.UpperBodyMaterial;
	}
	if (SlotDescription.Contains(TEXT("lower")) || SlotDescription.Contains(TEXT("pants"))
		|| SlotDescription.Contains(TEXT("jeans")))
	{
		return Appearance.LowerBodyMaterial;
	}
	if (SlotDescription.Contains(TEXT("shoe")))
	{
		return Appearance.ShoesMaterial;
	}
	return nullptr;
}

TSoftObjectPtr<USkeletalMesh> TestMesh(const TCHAR* AssetName)
{
	return TSoftObjectPtr<USkeletalMesh>(
		FSoftObjectPath(FString::Printf(TEXT("/Game/Mesh/Pedestrian/%s.%s"), AssetName, AssetName)));
}

TSoftObjectPtr<UMaterialInterface> TestMaterial(const TCHAR* AssetName)
{
	return TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(FString::Printf(TEXT("/Game/Mesh/Pedestrian/%s.%s"), AssetName, AssetName)));
}
}

AMTPedestrianBase* UMTPedestrianSpawnManager::SpawnPedestrian(
	UObject* WorldContextObject,
	TSubclassOf<AMTPedestrianBase> PedestrianClass,
	const FTransform& SpawnTransform,
	const FMTPedestrianGenerationConfig& Config,
	int32 RandomSeed)
{
	UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}

	if (!PedestrianClass)
	{
		PedestrianClass = AMTPedestrianBase::StaticClass();
	}

	FRandomStream RandomStream(RandomSeed == INDEX_NONE ? FMath::Rand() : RandomSeed);
	FMTPedestrianAppearance Appearance;
	if (!SelectAppearance(Config, RandomStream, Appearance))
	{
		UE_LOG(LogTemp, Error, TEXT("[PedestrianSpawnManager] 유효한 머리/상반신/하반신 Mesh 조합이 없습니다."));
		return nullptr;
	}

	AMTPedestrianBase* Pedestrian = World->SpawnActorDeferred<AMTPedestrianBase>(
		PedestrianClass,
		SpawnTransform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Pedestrian)
	{
		return nullptr;
	}

	UGameplayStatics::FinishSpawningActor(Pedestrian, SpawnTransform);
	Pedestrian->SetAppearance(Appearance);
	Pedestrian->SpawnDefaultController();
	return Pedestrian;
}

bool UMTPedestrianSpawnManager::ApplyAppearance(
	AMTPedestrianBase* Pedestrian,
	const FMTPedestrianAppearance& Appearance)
{
	if (!Pedestrian || !Appearance.HasRequiredMeshes())
	{
		return false;
	}

	UWorld* World = Pedestrian->GetWorld();
	UMTPedestrianMeshCacheSubsystem* Cache = World
		? World->GetSubsystem<UMTPedestrianMeshCacheSubsystem>()
		: nullptr;
	const FString CacheKey = Appearance.BuildMeshCacheKey();
	USkeletalMesh* MergedMesh = Cache ? Cache->FindMesh(CacheKey) : nullptr;

	if (!MergedMesh)
	{
		FSkeletalMeshMergeParams MergeParams;
		// 상반신을 첫 Mesh로 둬 전체 Skeleton, Animation Blueprint, Physics 기준으로 사용한다.
		MergeParams.MeshesToMerge = {
			Appearance.UpperBodyMesh,
			Appearance.LowerBodyMesh,
			Appearance.HeadMesh
		};
		MergeParams.Skeleton = Appearance.UpperBodyMesh->GetSkeleton();
		MergeParams.bSkeletonBefore = MergeParams.Skeleton != nullptr;
		MergeParams.bNeedsCpuAccess = false;
		MergeParams.StripTopLODS = 0;

		MergedMesh = USkeletalMergingLibrary::MergeMeshes(MergeParams);
		if (!MergedMesh)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[PedestrianSpawnManager] Skeletal Mesh 병합 실패: %s"),
				*CacheKey);
			return false;
		}

		if (Cache)
		{
			Cache->StoreMesh(CacheKey, MergedMesh);
		}
	}

	USkeletalMeshComponent* MeshComponent = Pedestrian->GetMesh();
	if (!MeshComponent)
	{
		return false;
	}

	MeshComponent->SetSkeletalMesh(MergedMesh);
	const TArray<FSkeletalMaterial>& Materials = MergedMesh->GetMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < Materials.Num(); ++MaterialIndex)
	{
		const FSkeletalMaterial& MaterialSlot = Materials[MaterialIndex];
		const FString SlotDescription = FString::Printf(
			TEXT("%s %s"),
			*MaterialSlot.MaterialSlotName.ToString(),
			*GetNameSafe(MaterialSlot.MaterialInterface)).ToLower();

		if (UMaterialInterface* Replacement = SelectReplacementMaterial(SlotDescription, Appearance))
		{
			MeshComponent->SetMaterial(MaterialIndex, Replacement);
		}
	}

	return true;
}

bool UMTPedestrianSpawnManager::SelectAppearance(
	const FMTPedestrianGenerationConfig& Config,
	FRandomStream& RandomStream,
	FMTPedestrianAppearance& OutAppearance)
{
	const float MaleProbability = FMath::Clamp(Config.MaleProbability, 0.0f, 1.0f);
	const bool bSelectMale = RandomStream.FRand() < MaleProbability;

	const FMTPedestrianAppearancePool* Pool = bSelectMale ? &Config.Male : &Config.Female;
	OutAppearance.Gender = bSelectMale ? EMTPedestrianGender::Male : EMTPedestrianGender::Female;

	// 선택된 성별 풀이 비어 있으면 반대 성별 풀을 안전한 폴백으로 사용한다.
	if (!Pool->HasRequiredMeshes())
	{
		Pool = bSelectMale ? &Config.Female : &Config.Male;
		OutAppearance.Gender = bSelectMale ? EMTPedestrianGender::Female : EMTPedestrianGender::Male;
	}
	if (!Pool->HasRequiredMeshes())
	{
		return false;
	}

	OutAppearance.HeadMesh = LoadRandomAsset(Pool->HeadMeshes, RandomStream);
	OutAppearance.UpperBodyMesh = LoadRandomAsset(Pool->UpperBodyMeshes, RandomStream);
	OutAppearance.LowerBodyMesh = LoadRandomAsset(Pool->LowerBodyMeshes, RandomStream);
	OutAppearance.HairMaterial = LoadRandomAsset(Pool->HairMaterials, RandomStream);
	OutAppearance.SkinMaterial = LoadRandomAsset(Pool->SkinMaterials, RandomStream);
	OutAppearance.UpperBodyMaterial = LoadRandomAsset(Pool->UpperBodyMaterials, RandomStream);
	OutAppearance.LowerBodyMaterial = LoadRandomAsset(Pool->LowerBodyMaterials, RandomStream);
	OutAppearance.ShoesMaterial = LoadRandomAsset(Pool->ShoesMaterials, RandomStream);
	return OutAppearance.HasRequiredMeshes();
}

FMTPedestrianGenerationConfig UMTPedestrianSpawnManager::MakeTestConfiguration()
{
	FMTPedestrianGenerationConfig Config;
	Config.MaleProbability = 0.5f;

	FMTPedestrianAppearancePool TestPool;
	TestPool.HeadMeshes.Add(TestMesh(TEXT("TestPedestrian_Rigged_Head")));
	TestPool.UpperBodyMeshes.Add(TestMesh(TEXT("TestPedestrian_Rigged_Body")));
	TestPool.LowerBodyMeshes.Add(TestMesh(TEXT("TestPedestrian_Rigged_LowerBody")));
	TestPool.HairMaterials.Add(TestMaterial(TEXT("M_Hair")));
	TestPool.SkinMaterials.Add(TestMaterial(TEXT("M_Skin")));
	TestPool.UpperBodyMaterials.Add(TestMaterial(TEXT("M_UpperBody")));
	TestPool.LowerBodyMaterials.Add(TestMaterial(TEXT("M_LowerBody")));
	TestPool.ShoesMaterials.Add(TestMaterial(TEXT("M_Shoes")));

	// 현재 테스트 자산은 성별 변형이 하나뿐이다. 등록 UI 검증용으로 양쪽 풀에 공유한다.
	Config.Male = TestPool;
	Config.Female = TestPool;
	return Config;
}
