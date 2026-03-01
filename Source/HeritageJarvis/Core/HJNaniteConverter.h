#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HJNaniteConverter.generated.h"

class UProceduralMeshComponent;
class UStaticMeshComponent;
class UStaticMesh;
class URuntimeVirtualTexture;

/**
 * Conversion result data.
 */
USTRUCT(BlueprintType)
struct FNaniteConversionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) bool bSuccess = false;
	UPROPERTY(BlueprintReadOnly) int32 TriangleCount = 0;
	UPROPERTY(BlueprintReadOnly) int32 VertexCount = 0;
	UPROPERTY(BlueprintReadOnly) bool bNaniteEnabled = false;
	UPROPERTY(BlueprintReadOnly) float ConversionTimeMs = 0.f;
	UPROPERTY(BlueprintReadOnly) FString ErrorMessage;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNaniteConversionComplete, const FNaniteConversionResult&, Result);

/**
 * UHJNaniteConverter — Runtime ProceduralMesh→StaticMesh+Nanite converter.
 *
 * ProceduralMeshComponent doesn't support Nanite or proper lightmaps,
 * making it unsuitable for large procedural geometry (5km orbital rings,
 * large CAD assemblies). This component:
 *
 *   1. Extracts geometry from a ProceduralMeshComponent
 *   2. Builds a FMeshDescription and creates a transient UStaticMesh
 *   3. Enables Nanite on the resulting static mesh
 *   4. Spawns a UStaticMeshComponent to replace the procedural one
 *   5. Optionally assigns RuntimeVirtualTexture for proper lightmaps
 *
 * Conversion is triggered automatically for meshes exceeding the triangle
 * or size threshold, or manually via ConvertToNanite().
 */
UCLASS(ClassGroup=(HeritageJarvis), meta=(BlueprintSpawnableComponent))
class HERITAGEJARVIS_API UHJNaniteConverter : public UActorComponent
{
	GENERATED_BODY()

public:
	UHJNaniteConverter();

	// -------------------------------------------------------
	// Configuration
	// -------------------------------------------------------

	/** Minimum triangle count to trigger automatic conversion. Default 50k. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HJ|Nanite")
	int32 AutoConvertTriangleThreshold = 50000;

	/** Minimum bounding extent (cm) to trigger automatic conversion. Default 100000 (1km). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HJ|Nanite")
	float AutoConvertSizeThreshold = 100000.f;

	/** Enable Nanite on the converted static mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HJ|Nanite")
	bool bEnableNanite = true;

	/** Enable RuntimeVirtualTexture for proper lightmaps on large meshes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HJ|Nanite")
	bool bEnableRVT = true;

	/** RVT page size (higher = better quality, more VRAM). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HJ|Nanite")
	int32 RVTPageSize = 256;

	// -------------------------------------------------------
	// API
	// -------------------------------------------------------

	/**
	 * Convert a ProceduralMeshComponent to a Nanite-enabled StaticMeshComponent.
	 * The procedural mesh is hidden after successful conversion.
	 * Returns the new StaticMeshComponent (nullptr on failure).
	 */
	UFUNCTION(BlueprintCallable, Category = "HJ|Nanite")
	UStaticMeshComponent* ConvertToNanite(UProceduralMeshComponent* SourceMesh);

	/**
	 * Check whether a mesh qualifies for Nanite conversion based on thresholds.
	 */
	UFUNCTION(BlueprintCallable, Category = "HJ|Nanite")
	bool ShouldConvert(UProceduralMeshComponent* SourceMesh) const;

	/**
	 * Attempt automatic conversion if thresholds are met.
	 * Call this after mesh load completes. Returns the new component if converted,
	 * nullptr if thresholds not met or conversion failed.
	 */
	UFUNCTION(BlueprintCallable, Category = "HJ|Nanite")
	UStaticMeshComponent* TryAutoConvert(UProceduralMeshComponent* SourceMesh);

	/** Fired when conversion completes (success or failure). */
	UPROPERTY(BlueprintAssignable, Category = "HJ|Nanite")
	FOnNaniteConversionComplete OnConversionComplete;

	/** Last conversion result. */
	UPROPERTY(BlueprintReadOnly, Category = "HJ|Nanite")
	FNaniteConversionResult LastResult;

	/** The converted static mesh component (if any). */
	UPROPERTY(BlueprintReadOnly, Category = "HJ|Nanite")
	UStaticMeshComponent* ConvertedMeshComp = nullptr;

private:
	/**
	 * Build a FMeshDescription from procedural mesh section data.
	 * Returns true if the mesh description was built successfully.
	 */
	bool BuildMeshDescription(UProceduralMeshComponent* Source,
	                          struct FMeshDescription& OutMeshDesc) const;

	/** Create the transient UStaticMesh from a MeshDescription. */
	UStaticMesh* CreateStaticMeshFromDescription(struct FMeshDescription& MeshDesc,
	                                              UMaterialInterface* Material) const;

	/** Transfer material from procedural mesh to static mesh component. */
	void TransferMaterial(UProceduralMeshComponent* Source,
	                      UStaticMeshComponent* Dest) const;

	/** Set up RVT on the static mesh component. */
	void SetupRuntimeVirtualTexture(UStaticMeshComponent* MeshComp) const;
};
