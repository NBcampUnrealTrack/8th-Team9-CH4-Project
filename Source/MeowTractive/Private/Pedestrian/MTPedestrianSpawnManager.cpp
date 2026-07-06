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

bool SelectMeshOption(
	const TArray<FMTPedestrianMeshOption>& Options,
	FRandomStream& RandomStream,
	TObjectPtr<USkeletalMesh>& OutMesh,
	TArray<FMTPedestrianSelectedMaterial>& OutMaterials)
{
	if (Options.IsEmpty())
	{
		return false;
	}

	const int32 StartIndex = RandomStream.RandRange(0, Options.Num() - 1);
	for (int32 Offset = 0; Offset < Options.Num(); ++Offset)
	{
		const FMTPedestrianMeshOption& Option = Options[(StartIndex + Offset) % Options.Num()];
		USkeletalMesh* LoadedMesh = Option.Mesh.LoadSynchronous();
		if (!LoadedMesh)
		{
			continue;
		}

		OutMesh = LoadedMesh;
		for (const FMTPedestrianMaterialSlotOption& SlotOption : Option.MaterialSlots)
		{
			if (SlotOption.SlotName.IsNone())
			{
				continue;
			}

			if (UMaterialInterface* Material = LoadRandomAsset(SlotOption.Materials, RandomStream))
			{
				FMTPedestrianSelectedMaterial& Selected = OutMaterials.AddDefaulted_GetRef();
				Selected.SlotName = SlotOption.SlotName;
				Selected.Material = Material;
			}
		}
		return true;
	}
	return false;
}

TSoftObjectPtr<USkeletalMesh> TestMesh(const TCHAR* RelativePath)
{
	return TSoftObjectPtr<USkeletalMesh>(
		FSoftObjectPath(FString::Printf(
			TEXT("/Game/Fab/Modular_Character_Male_Integrated_to_UE4_ALS/ModularCharacterMale/%s.%s"),
			RelativePath,
			*FString(RelativePath).RightChop(FString(RelativePath).Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd) + 1))));
}

TSoftObjectPtr<UMaterialInterface> TestMaterial(const TCHAR* RelativePath)
{
	return TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(FString::Printf(
			TEXT("/Game/Fab/Modular_Character_Male_Integrated_to_UE4_ALS/ModularCharacterMale/%s.%s"),
			RelativePath,
			*FString(RelativePath).RightChop(FString(RelativePath).Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd) + 1))));
}

FMTPedestrianMeshOption TestMeshOption(const TCHAR* RelativePath)
{
	FMTPedestrianMeshOption Option;
	Option.Mesh = TestMesh(RelativePath);
	return Option;
}

void AddTestMaterial(
	FMTPedestrianMeshOption& Option,
	const FName SlotName,
	const TCHAR* RelativeMaterialPath)
{
	FMTPedestrianMaterialSlotOption& Slot = Option.MaterialSlots.AddDefaulted_GetRef();
	Slot.SlotName = SlotName;
	Slot.Materials.Add(TestMaterial(RelativeMaterialPath));
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
		UE_LOG(LogTemp, Error, TEXT("[PedestrianSpawnManager] 유효한 머리/상반신/하반신/양손 Mesh 조합이 없습니다."));
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
			Appearance.HeadMesh,
			Appearance.HandsMesh
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
		const FMTPedestrianSelectedMaterial* Selected = Appearance.SelectedMaterials.FindByPredicate(
			[&MaterialSlot](const FMTPedestrianSelectedMaterial& Candidate)
			{
				return Candidate.SlotName == MaterialSlot.MaterialSlotName;
			});
		if (Selected && Selected->Material)
		{
			MeshComponent->SetMaterial(MaterialIndex, Selected->Material);
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

	OutAppearance.SelectedMaterials.Reset();
	if (!SelectMeshOption(Pool->HeadOptions, RandomStream, OutAppearance.HeadMesh, OutAppearance.SelectedMaterials)
		|| !SelectMeshOption(Pool->UpperBodyOptions, RandomStream, OutAppearance.UpperBodyMesh, OutAppearance.SelectedMaterials)
		|| !SelectMeshOption(Pool->LowerBodyOptions, RandomStream, OutAppearance.LowerBodyMesh, OutAppearance.SelectedMaterials)
		|| !SelectMeshOption(Pool->HandsOptions, RandomStream, OutAppearance.HandsMesh, OutAppearance.SelectedMaterials))
	{
		return false;
	}
	return OutAppearance.HasRequiredMeshes();
}

FMTPedestrianGenerationConfig UMTPedestrianSpawnManager::MakeTestConfiguration()
{
	FMTPedestrianGenerationConfig Config;
	Config.MaleProbability = 0.5f;

	FMTPedestrianAppearancePool TestPool;
	FMTPedestrianMeshOption Head = TestMeshOption(TEXT("meshes/head_N_1"));
	AddTestMaterial(Head, TEXT("M_Body1_Inst"), TEXT("clothing/M_Body1_Inst"));
	TestPool.HeadOptions.Add(MoveTemp(Head));

	FMTPedestrianMeshOption UpperBody = TestMeshOption(TEXT("meshes/torso_01"));
	AddTestMaterial(UpperBody, TEXT("M_blazer"), TEXT("clothing/M_blazer"));
	AddTestMaterial(UpperBody, TEXT("M_shirt2"), TEXT("clothing/M_shirt2"));
	AddTestMaterial(UpperBody, TEXT("M_Body1_Inst"), TEXT("clothing/M_Body1_Inst"));
	TestPool.UpperBodyOptions.Add(MoveTemp(UpperBody));

	FMTPedestrianMeshOption LowerBody = TestMeshOption(TEXT("meshes/legs_01"));
	AddTestMaterial(LowerBody, TEXT("M_Belt"), TEXT("clothing/M_Belt"));
	AddTestMaterial(LowerBody, TEXT("M_pant"), TEXT("clothing/M_pant"));
	AddTestMaterial(LowerBody, TEXT("M_shoes"), TEXT("clothing/M_shoes"));
	TestPool.LowerBodyOptions.Add(MoveTemp(LowerBody));

	FMTPedestrianMeshOption Hands = TestMeshOption(TEXT("meshes/hands_N_1"));
	AddTestMaterial(Hands, TEXT("M_Body1_Inst"), TEXT("clothing/M_Body1_Inst"));
	TestPool.HandsOptions.Add(MoveTemp(Hands));

	// 현재 테스트 자산은 성별 변형이 하나뿐이다. 등록 UI 검증용으로 양쪽 풀에 공유한다.
	Config.Male = TestPool;
	Config.Female = TestPool;
	return Config;
}
