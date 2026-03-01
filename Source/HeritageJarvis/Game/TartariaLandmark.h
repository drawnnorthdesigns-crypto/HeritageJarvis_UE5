#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TartariaTypes.h"
#include "TartariaLandmark.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

/**
 * ATartariaLandmark -- Tall landmark silhouette actor for each biome zone (Task #208).
 *
 * Placed at biome centers to provide long-range visual navigation cues.
 * Each biome has a distinctive compound-primitive silhouette assembled from
 * Engine/BasicShapes (Cube, Cylinder, Sphere), a matching beacon light
 * visible from extreme distance, and a dark silhouette material with
 * subtle emissive edge matching the beacon color.
 *
 * Biome silhouettes:
 *   CLEARINGHOUSE  -- Tall obelisk with sphere cap (gold beacon)
 *   SCRIPTORIUM    -- Twisted crystal spire (amber beacon)
 *   MONOLITH_WARD  -- Massive flat monolith slab (cold blue beacon)
 *   FORGE_DISTRICT -- Industrial chimney with smoke ball (orange beacon)
 *   VOID_REACH     -- Inverted cone with floating sphere, pulsing (purple beacon)
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaLandmark : public AActor
{
	GENERATED_BODY()

public:
	ATartariaLandmark();

	virtual void Tick(float DeltaTime) override;

	// -------------------------------------------------------
	// Properties
	// -------------------------------------------------------

	/** Root scene component for hierarchy management. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Landmark")
	USceneComponent* LandmarkRoot;

	/** Primary mesh component (base shape of the landmark). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Landmark")
	UStaticMeshComponent* LandmarkMesh;

	/** Biome this landmark represents -- set in editor or by WorldPopulator. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Landmark")
	ETartariaBiome OwningBiome = ETartariaBiome::Clearinghouse;

	/** Total height of the landmark in UE units (100 UU = 1m). Default 2000 = 20m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Landmark", meta = (ClampMin = "500.0"))
	float LandmarkHeight = 2000.f;

	/** Beacon point light -- high intensity, long attenuation, visible from far away. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Landmark")
	UPointLightComponent* BeaconLight;

	/**
	 * Configure silhouette geometry, material, and beacon color based on OwningBiome.
	 * Call after setting OwningBiome. Safe to call multiple times (clears previous geometry).
	 */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Landmark")
	void ConfigureForBiome();

protected:
	virtual void BeginPlay() override;

private:
	// ── Geometry helpers ─────────────────────────────────────────────

	/**
	 * Spawn and attach a BasicShapes primitive as a sub-component.
	 * @param Name         Unique component name
	 * @param ShapePath    Engine content path (e.g. /Engine/BasicShapes/Cube)
	 * @param RelLoc       Location relative to LandmarkRoot
	 * @param RelRot       Rotation relative to LandmarkRoot
	 * @param Scale        Scale3D
	 * @param BaseColor    Base surface color (dark silhouette)
	 * @param EmissiveColor Emissive edge glow color (matches beacon)
	 * @param EmissiveStrength Emissive intensity multiplier
	 * @return             The created mesh component (tracked in SpawnedMeshes)
	 */
	UStaticMeshComponent* AddLandmarkPrimitive(
		const FName& Name, const TCHAR* ShapePath,
		const FVector& RelLoc, const FRotator& RelRot, const FVector& Scale,
		const FLinearColor& BaseColor, const FLinearColor& EmissiveColor,
		float EmissiveStrength = 0.5f);

	/** Remove all previously spawned sub-mesh components. */
	void ClearSpawnedMeshes();

	// ── Build methods per biome ──────────────────────────────────────

	void BuildClearinghouseObelisk(float HeightScale);
	void BuildScriptoriumSpire(float HeightScale);
	void BuildMonolithWardSlab(float HeightScale);
	void BuildForgeDistrictChimney(float HeightScale);
	void BuildVoidReachObelisk(float HeightScale);

	// ── State ────────────────────────────────────────────────────────

	/** All dynamically spawned sub-mesh components for cleanup. */
	UPROPERTY()
	TArray<UStaticMeshComponent*> SpawnedMeshes;

	/** Dynamic materials on spawned meshes (for pulsing effects). */
	UPROPERTY()
	TArray<UMaterialInstanceDynamic*> SpawnedMaterials;

	/** Beacon color matching the owning biome. */
	FLinearColor BeaconColor = FLinearColor(1.0f, 0.85f, 0.4f);

	/** Base beacon intensity (before pulsing). */
	float BaseBeaconIntensity = 50000.f;

	/** Beacon attenuation radius. */
	float BeaconAttenuationRadius = 10000.f;

	// ── Void Reach pulsing state ─────────────────────────────────────

	/** Time accumulator for Void Reach beacon pulse oscillation. */
	float VoidPulseAccumulator = 0.f;

	/** Whether this landmark is configured for the pulsing Void biome. */
	bool bVoidPulseActive = false;

	/** Floating sphere component for Void Reach (offset/pulsing). */
	UPROPERTY()
	UStaticMeshComponent* VoidFloatingSphere = nullptr;

	/** Base Z offset for the floating void sphere (for bobbing). */
	float VoidSphereBaseZ = 0.f;
};
