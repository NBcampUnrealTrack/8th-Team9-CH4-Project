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
struct MEOWTRACTIVE_API FMTPedestrianAppearancePool
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pedestrian|Mesh")
	TArray<TSoftObjectPtr<USkeletalMesh>> HeadMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pedestrian|Mesh")
	TArray<TSoftObjectPtr<USkeletalMesh>> UpperBodyMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pedestrian|Mesh")
	TArray<TSoftObjectPtr<USkeletalMesh>> LowerBodyMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pedestrian|Material")
	TArray<TSoftObjectPtr<UMaterialInterface>> HairMaterials;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pedestrian|Material")
	TArray<TSoftObjectPtr<UMaterialInterface>> SkinMaterials;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pedestrian|Material")
	TArray<TSoftObjectPtr<UMaterialInterface>> UpperBodyMaterials;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pedestrian|Material")
	TArray<TSoftObjectPtr<UMaterialInterface>> LowerBodyMaterials;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pedestrian|Material")
	TArray<TSoftObjectPtr<UMaterialInterface>> ShoesMaterials;

	bool HasRequiredMeshes() const
	{
		return !HeadMeshes.IsEmpty() && !UpperBodyMeshes.IsEmpty() && !LowerBodyMeshes.IsEmpty();
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
	TObjectPtr<UMaterialInterface> HairMaterial;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedestrian")
	TObjectPtr<UMaterialInterface> SkinMaterial;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedestrian")
	TObjectPtr<UMaterialInterface> UpperBodyMaterial;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedestrian")
	TObjectPtr<UMaterialInterface> LowerBodyMaterial;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pedestrian")
	TObjectPtr<UMaterialInterface> ShoesMaterial;

	bool HasRequiredMeshes() const
	{
		return HeadMesh && UpperBodyMesh && LowerBodyMesh;
	}

	FString BuildMeshCacheKey() const
	{
		return FString::Printf(
			TEXT("%s|%s|%s"),
			*GetPathNameSafe(UpperBodyMesh),
			*GetPathNameSafe(LowerBodyMesh),
			*GetPathNameSafe(HeadMesh));
	}
};
