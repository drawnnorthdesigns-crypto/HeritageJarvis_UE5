#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProceduralMeshComponent.h"
#include "HJMeshLoader.generated.h"

/**
 * Delegate fired when mesh loading completes (success or failure).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMeshLoaded, bool, bSuccess, const FString&, StatusMessage);

/**
 * Delegate fired when mesh metadata is received.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMeshMetadata, const FString&, MetadataJson);

/**
 * UHJMeshLoader
 *
 * Runtime mesh import component for loading CAD-generated geometry from
 * the Flask backend into UE5 ProceduralMeshComponent.
 *
 * Supports three fetch modes:
 *   1. JSON mode — for small meshes (<50k triangles), fetches vertex/index
 *      arrays as JSON via /api/mesh/json/
 *   2. Binary mode — for large meshes, fetches packed binary buffers via
 *      /api/mesh/binary/ with custom header format
 *   3. GLB mode — for Nanite import, downloads .glb file via /api/mesh/glb/
 *
 * Usage:
 *   - Attach to an Actor alongside UProceduralMeshComponent
 *   - Call LoadMeshFromProject() with project ID and component name
 *   - Listen to OnMeshLoaded for completion
 *
 * The component handles:
 *   - Async HTTP fetch from Flask backend
 *   - Binary buffer parsing (vertex/normal/index/UV unpacking)
 *   - LOD selection based on distance or manual override
 *   - Automatic material assignment (default or Tartarian themed)
 *   - Bounding box metadata for physics/collision setup
 */
UCLASS(ClassGroup=(HeritageJarvis), meta=(BlueprintSpawnableComponent))
class HERITAGEJARVIS_API UHJMeshLoader : public UActorComponent
{
	GENERATED_BODY()

public:
	UHJMeshLoader();

	// ── Configuration ──────────────────────────────────────────

	/** LOD level to request (lod0 = full, lod3 = 10% triangles). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh|Config")
	FString LODLevel = TEXT("lod0");

	/** Max triangles for JSON mode. Above this, auto-switches to binary. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh|Config")
	int32 MaxJsonTriangles = 50000;

	/** Whether to use binary mode (more efficient for large meshes). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh|Config")
	bool bPreferBinaryMode = true;

	/** Target ProceduralMeshComponent (auto-found on owning actor if null). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh|Config")
	UProceduralMeshComponent* TargetMeshComponent;

	/** Material to apply to loaded mesh (optional). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh|Config")
	UMaterialInterface* MeshMaterial;

	/** Enable collision on the procedural mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh|Config")
	bool bEnableCollision = true;

	// ── Events ─────────────────────────────────────────────────

	/** Fired when mesh loading completes or fails. */
	UPROPERTY(BlueprintAssignable, Category = "Mesh|Events")
	FOnMeshLoaded OnMeshLoaded;

	/** Fired when mesh metadata is received (before geometry). */
	UPROPERTY(BlueprintAssignable, Category = "Mesh|Events")
	FOnMeshMetadata OnMeshMetadata;

	// ── Status ─────────────────────────────────────────────────

	/** Whether a mesh is currently being loaded. */
	UPROPERTY(BlueprintReadOnly, Category = "Mesh|Status")
	bool bIsLoading = false;

	/** Current project ID being loaded. */
	UPROPERTY(BlueprintReadOnly, Category = "Mesh|Status")
	FString CurrentProjectId;

	/** Current component name being loaded. */
	UPROPERTY(BlueprintReadOnly, Category = "Mesh|Status")
	FString CurrentComponentName;

	/** Triangle count of last loaded mesh. */
	UPROPERTY(BlueprintReadOnly, Category = "Mesh|Status")
	int32 LoadedTriangleCount = 0;

	/** Vertex count of last loaded mesh. */
	UPROPERTY(BlueprintReadOnly, Category = "Mesh|Status")
	int32 LoadedVertexCount = 0;

	// ── Blueprint Callable ─────────────────────────────────────

	/**
	 * Load mesh from a Heritage Jarvis project.
	 * Fetches optimized mesh data from Flask and creates procedural geometry.
	 *
	 * @param ProjectId   The project UUID (or prefix)
	 * @param ComponentName  The component name (e.g., "bracket", "enclosure")
	 */
	UFUNCTION(BlueprintCallable, Category = "Mesh")
	void LoadMeshFromProject(const FString& ProjectId, const FString& ComponentName);

	/**
	 * Load mesh from a direct STL path on the server.
	 *
	 * @param StlPath  Absolute path to STL file on the Flask server
	 */
	UFUNCTION(BlueprintCallable, Category = "Mesh")
	void LoadMeshFromPath(const FString& StlPath);

	/**
	 * Fetch mesh metadata without loading geometry.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mesh")
	void FetchMeshMetadata(const FString& ProjectId, const FString& ComponentName);

	/**
	 * Change LOD level and reload if a mesh is loaded.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mesh")
	void SetLOD(const FString& NewLOD);

	/**
	 * Clear the currently loaded mesh.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mesh")
	void ClearMesh();

	/**
	 * Check if the mesh pipeline is available on the server.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mesh")
	void CheckPipelineAvailable();

	/**
	 * Static utility: parse a JSON mesh response and apply it to any
	 * ProceduralMeshComponent without needing a full loader instance.
	 * Used by weapon equip flow on TartariaCharacter.
	 *
	 * @param TargetComp  The ProceduralMeshComponent to populate.
	 * @param JsonString  The raw JSON string from /api/mesh/json endpoint.
	 * @return true on success, false on parse failure.
	 */
	static bool LoadMeshFromJsonString(UProceduralMeshComponent* TargetComp, const FString& JsonString);

	/**
	 * Apply a Tartarian material preset to a ProceduralMeshComponent.
	 * Presets: "bronze", "void_crystal", "aether_glow", "worn_stone", "dark_iron"
	 *
	 * @param TargetComp  The ProceduralMeshComponent to apply material to.
	 * @param PresetName  Material preset identifier.
	 */
	static void ApplyMaterialPreset(UProceduralMeshComponent* TargetComp, const FString& PresetName);

	/**
	 * Apply material preset based on weapon rarity tier.
	 * COMMON->worn_stone, UNCOMMON->bronze, RARE->dark_iron, EPIC->void_crystal, LEGENDARY->aether_glow
	 */
	static void ApplyRarityMaterial(UProceduralMeshComponent* TargetComp, const FString& Rarity);

	/**
	 * Play a reveal animation on a mesh actor: scales from 0 to target over 0.5s
	 * with a golden emissive flash. Call this after loading mesh geometry.
	 *
	 * @param MeshActor  The actor containing the mesh to animate.
	 */
	static void PlayMeshReveal(AActor* MeshActor);

protected:
	virtual void BeginPlay() override;

private:
	/** Find or create the target ProceduralMeshComponent. */
	UProceduralMeshComponent* GetOrCreateMeshComponent();

	/** Get the API client from GameInstance. */
	class UHJApiClient* GetApiClient() const;

	/** Fetch and apply mesh via JSON endpoint. */
	void FetchMeshJson(const FString& ProjectId, const FString& ComponentName);

	/** Fetch and apply mesh via binary endpoint. */
	void FetchMeshBinary(const FString& ProjectId, const FString& ComponentName);

	/** Parse JSON response and create procedural mesh. */
	void ApplyMeshFromJson(const FString& JsonResponse);

	/** Parse binary response and create procedural mesh. */
	void ApplyMeshFromBinary(const TArray<uint8>& BinaryData);

	/** Apply vertex/normal/index/UV data to ProceduralMeshComponent. */
	void CreateProceduralMesh(const TArray<FVector>& Vertices,
	                          const TArray<int32>& Triangles,
	                          const TArray<FVector>& Normals,
	                          const TArray<FVector2D>& UVs);

	/**
	 * Cook collision async: spawn mesh with NO collision first (instant render),
	 * then cook physics body on a background thread to avoid main-thread freeze.
	 * When cooking completes, enable collision on the game thread.
	 */
	void CookCollisionAsync(UProceduralMeshComponent* MeshComp);

	/** Whether async collision cooking is in progress. */
	bool bCookingCollision = false;
};
