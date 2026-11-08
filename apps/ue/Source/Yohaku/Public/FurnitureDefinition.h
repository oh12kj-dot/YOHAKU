#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FurnitureDefinition.generated.h"
class UStaticMesh;
class UMaterialInterface;
USTRUCT(BlueprintType)
struct FFurnitureMeshPart
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) TObjectPtr<UStaticMesh> Mesh;
    UPROPERTY(EditAnywhere) TObjectPtr<UMaterialInterface> Material;
    UPROPERTY(EditAnywhere) FTransform Transform;
};
UCLASS(BlueprintType)
class YOHAKU_API UFurnitureDefinition : public UDataAsset
{
    GENERATED_BODY()
  public:
    UPROPERTY(EditAnywhere) FString AssetVersionId;
    UPROPERTY(EditAnywhere) FVector DimensionsMm = FVector(1500, 870, 820);
    UPROPERTY(EditAnywhere) FString Provenance;
    UPROPERTY(EditAnywhere) bool bStudyCandidate = true;
    UPROPERTY(EditAnywhere) TArray<FFurnitureMeshPart> Parts;
};
