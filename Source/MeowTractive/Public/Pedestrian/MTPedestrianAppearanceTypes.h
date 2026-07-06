#pragma once

#include "CoreMinimal.h"
#include "MTPedestrianAppearanceTypes.generated.h"

class UMaterialInterface;
class USkeletalMesh;

UENUM(BlueprintType)
enum class EMTPedestrianGender : uint8
{
	Male,
	Female
};

USTRUCT(BlueprintType)
struct MEOWTRACTIVE_API FMTPedestrianMaterialSlotOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pedestrian|Material")
	FName SlotName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pedestrian|Material")
	TArray<TSoftObjectPtr<UMaterialInterface>> Materials;
};

USTRUCT(BlueprintType)
struct MEOWTRACTIVE_API FMTPedestrianMeshOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pedestrian|Mesh")
	TSoftObjectPtr<USkeletalMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pedestrian|Material")
	TArray<FMTPedestrianMaterialSlotOption> MaterialSlots;
};

USTRUCT(BlueprintType)
struct MEOWTRACTIVE_API FMTPedestrianSelectedMaterial
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedestrian|Material")
	FName SlotName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedestrian|Material")
	TObjectPtr<UMaterialInterface> Material;
};

USTRUCT(BlueprintType)
struct MEOWTRACTIVE_API FMTPedestrianAppearancePool
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pedestrian|Mesh")
	TArray<FMTPedestrianMeshOption> HeadOptions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pedestrian|Mesh")
	TArray<FMTPedestrianMeshOption> UpperBodyOptions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pedestrian|Mesh")
	TArray<FMTPedestrianMeshOption> LowerBodyOptions;

	// 현재 테스트 자산의 hands_N_1은 왼손과 오른손이 들어 있는 단일 Skeletal Mesh다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pedestrian|Mesh")
	TArray<FMTPedestrianMeshOption> HandsOptions;

	bool HasRequiredMeshes() const
	{
		return !HeadOptions.IsEmpty()
			&& !UpperBodyOptions.IsEmpty()
			&& !LowerBodyOptions.IsEmpty()
			&& !HandsOptions.IsEmpty();
	}
};

USTRUCT(BlueprintType)
struct MEOWTRACTIVE_API FMTPedestrianGenerationConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pedestrian", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaleProbability = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pedestrian")
	FMTPedestrianAppearancePool Male;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pedestrian")
	FMTPedestrianAppearancePool Female;
};

USTRUCT(BlueprintType)
struct MEOWTRACTIVE_API FMTPedestrianAppearance
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedestrian")
	EMTPedestrianGender Gender = EMTPedestrianGender::Male;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedestrian")
	TObjectPtr<USkeletalMesh> HeadMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedestrian")
	TObjectPtr<USkeletalMesh> UpperBodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedestrian")
	TObjectPtr<USkeletalMesh> LowerBodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedestrian")
	TObjectPtr<USkeletalMesh> HandsMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedestrian")
	TArray<FMTPedestrianSelectedMaterial> SelectedMaterials;

	bool HasRequiredMeshes() const
	{
		return HeadMesh && UpperBodyMesh && LowerBodyMesh && HandsMesh;
	}

	FString BuildMeshCacheKey() const
	{
		return FString::Printf(
			TEXT("%s|%s|%s|%s"),
			*GetPathNameSafe(UpperBodyMesh),
			*GetPathNameSafe(LowerBodyMesh),
			*GetPathNameSafe(HeadMesh),
			*GetPathNameSafe(HandsMesh));
	}
};
