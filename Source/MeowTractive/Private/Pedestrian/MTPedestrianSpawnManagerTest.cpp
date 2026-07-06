#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/SkeletalMesh.h"
#include "SkeletalMergingLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMTPedestrianMeshMergeTest,
	"MeowTractive.Pedestrian.MeshMerge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMTPedestrianMeshMergeTest::RunTest(const FString& Parameters)
{
	USkeletalMesh* Head = LoadObject<USkeletalMesh>(
		nullptr,
		TEXT("/Game/Fab/Modular_Character_Male_Integrated_to_UE4_ALS/ModularCharacterMale/meshes/head_N_1.head_N_1"));
	USkeletalMesh* UpperBody = LoadObject<USkeletalMesh>(
		nullptr,
		TEXT("/Game/Fab/Modular_Character_Male_Integrated_to_UE4_ALS/ModularCharacterMale/meshes/torso_01.torso_01"));
	USkeletalMesh* LowerBody = LoadObject<USkeletalMesh>(
		nullptr,
		TEXT("/Game/Fab/Modular_Character_Male_Integrated_to_UE4_ALS/ModularCharacterMale/meshes/legs_01.legs_01"));
	USkeletalMesh* Hands = LoadObject<USkeletalMesh>(
		nullptr,
		TEXT("/Game/Fab/Modular_Character_Male_Integrated_to_UE4_ALS/ModularCharacterMale/meshes/hands_N_1.hands_N_1"));

	TestNotNull(TEXT("Head test mesh must load"), Head);
	TestNotNull(TEXT("Upper-body test mesh must load"), UpperBody);
	TestNotNull(TEXT("Lower-body test mesh must load"), LowerBody);
	TestNotNull(TEXT("Both-hands test mesh must load"), Hands);
	if (!Head || !UpperBody || !LowerBody || !Hands)
	{
		return false;
	}

	FSkeletalMeshMergeParams MergeParams;
	MergeParams.MeshesToMerge = {UpperBody, LowerBody, Head, Hands};
	MergeParams.Skeleton = UpperBody->GetSkeleton();
	MergeParams.bSkeletonBefore = MergeParams.Skeleton != nullptr;

	USkeletalMesh* MergedMesh = USkeletalMergingLibrary::MergeMeshes(MergeParams);
	TestNotNull(TEXT("Configured pedestrian meshes must merge"), MergedMesh);
	if (MergedMesh)
	{
		TestTrue(TEXT("Merged mesh must preserve material slots"), !MergedMesh->GetMaterials().IsEmpty());
	}

	return !HasAnyErrors();
}

#endif
