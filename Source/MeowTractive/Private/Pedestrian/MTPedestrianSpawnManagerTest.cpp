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
		TEXT("/Game/Mesh/Pedestrian/TestPedestrian_Rigged_Head.TestPedestrian_Rigged_Head"));
	USkeletalMesh* UpperBody = LoadObject<USkeletalMesh>(
		nullptr,
		TEXT("/Game/Mesh/Pedestrian/TestPedestrian_Rigged_Body.TestPedestrian_Rigged_Body"));
	USkeletalMesh* LowerBody = LoadObject<USkeletalMesh>(
		nullptr,
		TEXT("/Game/Mesh/Pedestrian/TestPedestrian_Rigged_LowerBody.TestPedestrian_Rigged_LowerBody"));

	TestNotNull(TEXT("Head test mesh must load"), Head);
	TestNotNull(TEXT("Upper-body test mesh must load"), UpperBody);
	TestNotNull(TEXT("Lower-body test mesh must load"), LowerBody);
	if (!Head || !UpperBody || !LowerBody)
	{
		return false;
	}

	FSkeletalMeshMergeParams MergeParams;
	MergeParams.MeshesToMerge = {UpperBody, LowerBody, Head};
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
