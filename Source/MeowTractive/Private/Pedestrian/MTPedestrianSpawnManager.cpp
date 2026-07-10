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
TObjectType* SelectRandomAsset(const TArray<TObjectPtr<TObjectType>>& Assets, FRandomStream& RandomStream)
{
	if (Assets.IsEmpty())
	{
		return nullptr;
	}

	const int32 StartIndex = RandomStream.RandRange(0, Assets.Num() - 1);
	for (int32 Offset = 0; Offset < Assets.Num(); ++Offset)
	{
		const int32 Index = (StartIndex + Offset) % Assets.Num();
		if (TObjectType* Asset = Assets[Index].Get())
		{
			return Asset;
		}
	}
	return nullptr;
}

FName ResolveMaterialSlotName(const USkeletalMesh* Mesh, const FName ConfiguredName)
{
	if (!Mesh || ConfiguredName.IsNone())
	{
		return NAME_None;
	}

	const TArray<FSkeletalMaterial>& MeshMaterials = Mesh->GetMaterials();

	// 실제 Skeletal Mesh SlotName을 입력한 경우 그대로 사용한다.
	for (const FSkeletalMaterial& MeshMaterial : MeshMaterials)
	{
		if (MeshMaterial.MaterialSlotName == ConfiguredName)
		{
			return MeshMaterial.MaterialSlotName;
		}
	}

	// BP에서 Material asset 이름을 입력한 경우 해당 Material이 연결된 실제 SlotName으로 변환한다.
	for (const FSkeletalMaterial& MeshMaterial : MeshMaterials)
	{
		if (MeshMaterial.MaterialInterface
			&& MeshMaterial.MaterialInterface->GetFName() == ConfiguredName)
		{
			return MeshMaterial.MaterialSlotName;
		}
	}

	return NAME_None;
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
		USkeletalMesh* LoadedMesh = Option.Mesh.Get();
		if (!LoadedMesh)
		{
			continue;
		}

		OutMesh = LoadedMesh;
		for (const FMTPedestrianMaterialSlotOption& SlotOption : Option.MaterialSlots)
		{
			const FName ResolvedSlotName = ResolveMaterialSlotName(LoadedMesh, SlotOption.SlotName);
			if (ResolvedSlotName.IsNone())
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("[PedestrianSpawnManager] Material Slot을 찾을 수 없습니다. Mesh=%s ConfiguredName=%s"),
					*GetNameSafe(LoadedMesh),
					*SlotOption.SlotName.ToString());
				continue;
			}

			if (UMaterialInterface* Material = SelectRandomAsset(SlotOption.Materials, RandomStream))
			{
				FMTPedestrianSelectedMaterial& Selected = OutMaterials.AddDefaulted_GetRef();
				Selected.SlotName = ResolvedSlotName;
				Selected.Material = Material;
			}
		}
		return true;
	}
	return false;
}

USkeletalMesh* LoadTestMesh(const TCHAR* RelativePath)
{
	const FString RelativePathString(RelativePath);
	const FString AssetName = RelativePathString.Mid(
		RelativePathString.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd) + 1);
	const FString AssetPath = FString::Printf(
		TEXT("/Game/Fab/Modular_Character_Male_Integrated_to_UE4_ALS/ModularCharacterMale/%s.%s"),
		RelativePath,
		*AssetName);
	return LoadObject<USkeletalMesh>(nullptr, *AssetPath);
}

UMaterialInterface* LoadTestMaterial(const TCHAR* RelativePath)
{
	const FString RelativePathString(RelativePath);
	const FString AssetName = RelativePathString.Mid(
		RelativePathString.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd) + 1);
	const FString AssetPath = FString::Printf(
		TEXT("/Game/Fab/Modular_Character_Male_Integrated_to_UE4_ALS/ModularCharacterMale/%s.%s"),
		RelativePath,
		*AssetName);
	return LoadObject<UMaterialInterface>(nullptr, *AssetPath);
}

FMTPedestrianMeshOption TestMeshOption(const TCHAR* RelativePath)
{
	FMTPedestrianMeshOption Option;
	Option.Mesh = LoadTestMesh(RelativePath);
	return Option;
}

void AddTestMaterial(
	FMTPedestrianMeshOption& Option,
	const FName SlotName,
	const TCHAR* RelativeMaterialPath)
{
	FMTPedestrianMaterialSlotOption& Slot = Option.MaterialSlots.AddDefaulted_GetRef();
	Slot.SlotName = SlotName;
	Slot.Materials.Add(LoadTestMaterial(RelativeMaterialPath));
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

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMTPedestrianMaterialSlotResolutionTest,
	"MeowTractive.Pedestrian.MaterialSlotResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMTPedestrianMaterialSlotResolutionTest::RunTest(const FString& Parameters)
{
	USkeletalMesh* Legs02 = LoadObject<USkeletalMesh>(
		nullptr,
		TEXT("/Game/Fab/Modular_Character_Male_Integrated_to_UE4_ALS/ModularCharacterMale/meshes/legs_02.legs_02"));
	USkeletalMesh* Legs03 = LoadObject<USkeletalMesh>(
		nullptr,
		TEXT("/Game/Fab/Modular_Character_Male_Integrated_to_UE4_ALS/ModularCharacterMale/meshes/legs_03.legs_03"));

	TestNotNull(TEXT("legs_02 must load"), Legs02);
	TestNotNull(TEXT("legs_03 must load"), Legs03);
	if (!Legs02 || !Legs03)
	{
		return false;
	}

	TestEqual(
		TEXT("legs_02 material asset name must resolve to actual shoe slot"),
		ResolveMaterialSlotName(Legs02, TEXT("M_shoes01")),
		FName(TEXT("M_Shoe02")));
	TestEqual(
		TEXT("legs_03 material asset name must resolve to actual shoe slot"),
		ResolveMaterialSlotName(Legs03, TEXT("M_shoes03")),
		FName(TEXT("M_Shoe02")));
	TestEqual(
		TEXT("actual slot name must remain valid"),
		ResolveMaterialSlotName(Legs02, TEXT("M_Shoe02")),
		FName(TEXT("M_Shoe02")));

	TArray<TObjectPtr<UMaterialInterface>> ShoeMaterials = {
		LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Game/Fab/Modular_Character_Male_Integrated_to_UE4_ALS/ModularCharacterMale/clothing/M_shoes01.M_shoes01")),
		LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Game/Fab/Modular_Character_Male_Integrated_to_UE4_ALS/ModularCharacterMale/clothing/M_shoes03.M_shoes03"))
	};
	TSet<FName> SelectedMaterialNames;
	for (int32 Seed = 0; Seed < 32; ++Seed)
	{
		FRandomStream RandomStream(Seed);
		if (UMaterialInterface* Selected = SelectRandomAsset(ShoeMaterials, RandomStream))
		{
			SelectedMaterialNames.Add(Selected->GetFName());
		}
	}
	TestTrue(TEXT("both configured shoe materials must be selected"), SelectedMaterialNames.Num() == 2);

	USkeletalMesh* Head = LoadObject<USkeletalMesh>(
		nullptr,
		TEXT("/Game/Fab/Modular_Character_Male_Integrated_to_UE4_ALS/ModularCharacterMale/meshes/head_N_1.head_N_1"));
	USkeletalMesh* UpperBody = LoadObject<USkeletalMesh>(
		nullptr,
		TEXT("/Game/Fab/Modular_Character_Male_Integrated_to_UE4_ALS/ModularCharacterMale/meshes/torso_01.torso_01"));
	USkeletalMesh* Hands = LoadObject<USkeletalMesh>(
		nullptr,
		TEXT("/Game/Fab/Modular_Character_Male_Integrated_to_UE4_ALS/ModularCharacterMale/meshes/hands_N_1.hands_N_1"));
	TestNotNull(TEXT("head must load"), Head);
	TestNotNull(TEXT("upper body must load"), UpperBody);
	TestNotNull(TEXT("hands must load"), Hands);
	if (!Head || !UpperBody || !Hands)
	{
		return false;
	}

	FSkeletalMeshMergeParams MergeParams;
	MergeParams.MeshesToMerge = {UpperBody, Legs02, Head, Hands};
	MergeParams.Skeleton = UpperBody->GetSkeleton();
	MergeParams.bSkeletonBefore = MergeParams.Skeleton != nullptr;
	USkeletalMesh* MergedMesh = USkeletalMergingLibrary::MergeMeshes(MergeParams);
	TestNotNull(TEXT("legs_02 combination must merge"), MergedMesh);
	if (MergedMesh)
	{
		const bool bHasResolvedShoeSlot = MergedMesh->GetMaterials().ContainsByPredicate(
			[](const FSkeletalMaterial& Material)
			{
				return Material.MaterialSlotName == TEXT("M_Shoe02");
			});
		TestTrue(TEXT("merged mesh must preserve resolved shoe slot"), bHasResolvedShoeSlot);
	}

	return !HasAnyErrors();
}

#endif
