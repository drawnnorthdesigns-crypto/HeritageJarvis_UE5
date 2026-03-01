#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HJInteractable.h"
#include "TartariaTypes.h"
#include "TartariaForgeBuilding.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UAudioComponent;
class UProceduralMeshComponent;
class UHJMeshLoader;
class UTextRenderComponent;
class UMaterialInstanceDynamic;

/**
 * ATartariaForgeBuilding — Interactable building actor in the Tartaria world.
 * Types: Forge, Scriptorium, Treasury, Barracks, Lab.
 * OnInteract: opens building-specific UI or navigates CEF to building route.
 *
 * Visuals: Multi-mesh compound silhouettes using Engine/BasicShapes primitives
 * assembled to create recognizable architectural forms per building type.
 * Call SetupBuildingVisuals() after setting BuildingType to construct the geometry.
 *
 * Dynamic Effects (Forge type only):
 * - Emissive glow pulse synced to pipeline activity
 * - Chimney light flicker simulating smoke/fire
 * - Anvil sound cue on stage transitions
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaForgeBuilding : public AActor, public IHJInteractable
{
	GENERATED_BODY()

public:
	ATartariaForgeBuilding();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// -------------------------------------------------------
	// IHJInteractable
	// -------------------------------------------------------

	virtual void OnInteract_Implementation(APlayerController* Interactor) override;
	virtual FString GetInteractPrompt_Implementation() const override;

	// -------------------------------------------------------
	// Properties
	// -------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Building")
	FString BuildingId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Building")
	FString BuildingName = TEXT("Unnamed Building");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Building")
	ETartariaBuildingType BuildingType = ETartariaBuildingType::Forge;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Building")
	int32 BuildingLevel = 1;

	// -------------------------------------------------------
	// Forge Activity State — driven by pipeline polling
	// -------------------------------------------------------

	/** Set to true when the pipeline is actively running */
	UPROPERTY(BlueprintReadWrite, Category = "Tartaria|Forge")
	bool bForgeActive = false;

	/** Current pipeline stage name (for anvil cue timing) */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Forge")
	FString CurrentForgeStage;

	/** Called by GameMode when pipeline status updates */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Forge")
	void SetForgeActive(bool bActive, const FString& StageName);

	/** Called by GameMode on stage transitions — triggers anvil sound */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Forge")
	void OnStageTransition(const FString& NewStage, int32 StageIndex);

	/** Update production state from backend world-consequence data.
	 *  When active: doubles building light intensity to show economic activity.
	 *  When idle: returns to baseline intensity.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Forge")
	void SetProductionActive(bool bActive);

	// -------------------------------------------------------
	// Components — Primary mesh + decorative elements
	// -------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Building")
	UStaticMeshComponent* BuildingMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Building")
	UPointLightComponent* BuildingLight;

	/** Chimney flicker light — visible when forge is active */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Forge")
	UPointLightComponent* ChimneyFlickerLight;

	/**
	 * Build compound architectural silhouette based on BuildingType.
	 * Call after setting BuildingType. Creates additional mesh components
	 * attached to the root to form recognizable building shapes.
	 * @param ScaleMultiplier - 1.0 for primary, 0.6 for secondary outposts
	 */
	void SetupBuildingVisuals(float ScaleMultiplier = 1.0f);

	// -------------------------------------------------------
	// Blueprint events
	// -------------------------------------------------------

	UFUNCTION(BlueprintImplementableEvent, Category = "Tartaria|Building")
	void OnBuildingInteracted(APlayerController* Interactor);

	UFUNCTION(BlueprintImplementableEvent, Category = "Tartaria|Forge")
	void OnForgeStageChanged(const FString& NewStage, int32 StageIndex);

	// -------------------------------------------------------
	// Display Pedestal — shows last forged model in-world
	// -------------------------------------------------------

	/** Pedestal base mesh (stone cylinder in front of forge) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Forge")
	UStaticMeshComponent* PedestalMesh;

	/** ProceduralMesh for displaying the forged CAD model */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Forge")
	UProceduralMeshComponent* DisplayModelMesh;

	/** Mesh loader component for fetching model from Flask */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Forge")
	UHJMeshLoader* MeshLoader;

	/** Spotlight illuminating the display model */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Forge")
	UPointLightComponent* PedestalLight;

	/** Whether a forged model is currently displayed */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Forge")
	bool bModelDisplayed = false;

	/** Project ID of the currently displayed model */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Forge")
	FString DisplayedProjectId;

	/** Project name for the interact prompt */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Forge")
	FString DisplayedProjectName;

	/**
	 * Fetch and display the latest forged model on the pedestal.
	 * Called automatically when pipeline completes or on BeginPlay.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Forge")
	void LoadLatestForgedModel();

	/** Called when mesh loader finishes — lights up pedestal */
	UFUNCTION()
	void OnDisplayModelLoaded(bool bSuccess, const FString& StatusMessage);

	/** Slow rotation accumulator for display model */
	float DisplayRotation = 0.0f;

	UFUNCTION(BlueprintCallable)
	void StartConstruction();

	// -------------------------------------------------------
	// Materialization Burst — VFX on pipeline completion
	// -------------------------------------------------------

	/**
	 * Play materialization burst VFX when a pipeline project completes.
	 * Flashes building emissive to bright gold, scales up pedestal briefly,
	 * and activates a golden spark light burst.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Forge")
	void PlayMaterializationBurst();

	/** Extra light for completion burst (golden upward flash) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Forge")
	UPointLightComponent* CompletionBurstLight;

	// -------------------------------------------------------
	// Forge Queue Counter — Progress Light + Floating Text
	// -------------------------------------------------------

	/** Progress-driven chimney light — intensity maps to queue progress_pct */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Forge")
	UPointLightComponent* ChimneyLight;

	/** Floating text above the building showing queue status */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Forge")
	UTextRenderComponent* ProgressText;

	/** Cached queue status fields (updated from API poll) */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Forge")
	int32 QueueCompletedToday = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Forge")
	int32 QueueTotalJobs = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Forge")
	int32 QueueProgressPct = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Forge")
	FString QueueActiveStageName;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Forge")
	bool bQueueHasActiveJob = false;

	// -------------------------------------------------------
	// Live-Thinking Proxy — real-time proxy geometry during generation
	// -------------------------------------------------------

	/** Dynamic array of proxy geometry primitives currently displayed */
	UPROPERTY()
	TArray<UStaticMeshComponent*> ProxyPrimitives;

	/** Whether proxy generation display is in progress */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Forge|Proxy")
	bool bProxyActive = false;

	/** Translucent golden material applied to proxy primitives */
	UPROPERTY()
	UMaterialInstanceDynamic* ProxyMaterial = nullptr;

	/** Poll the live-proxy endpoint and spawn/update proxy geometry */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Forge|Proxy")
	void PollLiveProxy();

	/** Remove all proxy primitives from the scene */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Forge|Proxy")
	void ClearProxyGeometry();

	/**
	 * Spawn a single proxy primitive (box/cylinder/sphere) attached to the forge.
	 * Coordinates are in Python mm space — converted to UE5 cm internally.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Forge|Proxy")
	void AddProxyPrimitive(const FString& Type, FVector Location, FVector Scale, FRotator Rotation);

	/**
	 * Transition proxy primitives from translucent gold to opaque on completion.
	 * Called when pipeline status transitions to "complete".
	 */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Forge|Proxy")
	void SolidifyProxy();

	// -------------------------------------------------------
	// Ember Particle Pool — chimney spark effect
	// -------------------------------------------------------

	/** Current ember spawn rate (particles per second). Scales with forge activity. */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Forge|Embers")
	float EmberSpawnRate = 5.f;

private:
	// --- Ember Particle Pool ---
	static constexpr int32 MAX_EMBERS = 20;

	/** Pre-allocated sphere meshes used as ember particles. */
	UPROPERTY()
	TArray<UStaticMeshComponent*> EmberPool;

	/** Per-ember dynamic materials for color/emissive fading. */
	UPROPERTY()
	TArray<UMaterialInstanceDynamic*> EmberMaterials;

	/** Per-ember remaining lifetime (<=0 means inactive/available). */
	TArray<float> EmberLifetimes;

	/** Per-ember max lifetime (for fade calculation). */
	TArray<float> EmberMaxLifetimes;

	/** Per-ember velocity (cm/s) — upward + random drift. */
	TArray<FVector> EmberVelocities;

	/** Per-ember initial scale (randomized on spawn). */
	TArray<FVector> EmberScales;

	/** Accumulator for spawn timing. */
	float EmberSpawnAccumulator = 0.f;

	/** Initialize the ember mesh pool (called from BeginPlay). */
	void InitEmberPool();

	/** Activate one ember from the pool at the chimney top. */
	void SpawnEmber();

	/** Tick all active embers: move, fade, recycle. */
	void UpdateEmbers(float DeltaTime);

	/** Set ember spawn rate based on forge activity (called from ApplyQueueVisuals). */
	void SetEmberSpawnRate(float Rate);

	// --- Proxy Primitive Object Pool ---
	static constexpr int32 PROXY_POOL_SIZE = 50;

	UPROPERTY()
	TArray<UStaticMeshComponent*> ProxyPool;

	/** Bitfield tracking which pool slots are in use. */
	TArray<bool> ProxyPoolInUse;

	/** Number of currently active (visible) proxy primitives. */
	int32 ActiveProxyCount = 0;

	/** Whether pool-exhausted warning has already been logged this session. */
	bool bPoolExhaustedWarned = false;

	/** Initialize the pool (called from BeginPlay). */
	void InitProxyPool();

	/** Acquire a mesh component from the pool. Returns nullptr if pool exhausted. */
	UStaticMeshComponent* AcquireProxyPrimitive();

	/** Release a mesh component back to the pool (hides it, resets transform). */
	void ReleaseProxyPrimitive(UStaticMeshComponent* Comp);

	/** Release all active proxy primitives back to pool. */
	void ReleaseAllProxies();

	/** Timer accumulator for queue status polling (every 5 seconds) */
	float QueuePollTimer = 0.0f;

	/** Poll the backend queue-status endpoint and update light/text */
	void PollQueueStatus();

	/** Apply queue status data to ChimneyLight and ProgressText */
	void ApplyQueueVisuals();

	// ── Live Proxy State ──────────────────────────────────────
	/** Timer accumulator for proxy polling (every 2 seconds) */
	float ProxyPollTimer = 0.0f;

	/** Previous bQueueHasActiveJob state for detecting transitions */
	bool bPreviousQueueHasActiveJob = false;

	/** Whether proxy solidification is in progress */
	bool bSolidifyActive = false;

	/** Elapsed time into solidification animation */
	float SolidifyElapsed = 0.0f;

	/** Total duration of solidification effect (seconds) */
	float SolidifyDuration = 0.5f;

	/** Timer until proxy is cleared after solidification (seconds) */
	float ProxyClearCountdown = -1.0f;

	/** Scale-up animation tracking per proxy primitive */
	struct FProxyScaleAnim
	{
		int32 Index;
		FVector TargetScale;
		float Elapsed;
		float Duration;
	};
	TArray<FProxyScaleAnim> ProxyScaleAnims;
	// Construction animation state
	bool bIsConstructing = false;
	float ConstructionProgress = 1.0f;  // 0=unbuilt, 1=complete
	float ConstructionDuration = 3.0f;   // seconds to build
	float ConstructionTimer = 0.0f;

	/** Helper: attach a primitive shape mesh as a sub-component. */
	UStaticMeshComponent* AddPrimitiveMesh(const FName& Name, const TCHAR* ShapePath,
		const FVector& RelLoc, const FRotator& RelRot, const FVector& Scale,
		const FLinearColor& Color);

	/** Helper: attach an emissive "window" strip. */
	UStaticMeshComponent* AddWindowStrip(const FName& Name,
		const FVector& RelLoc, const FRotator& RelRot, const FVector& Scale,
		const FLinearColor& EmissiveColor);

	// -------------------------------------------------------
	// Forge Glow State
	// -------------------------------------------------------

	/** Accumulated time for glow pulse */
	float GlowAccumulator = 0.0f;

	/** Base intensity of the forge light (set during SetupBuildingVisuals) */
	float BaseForgeIntensity = 8000.0f;

	/** Base glow color (warm orange) */
	FLinearColor BaseForgeColor;

	/** Tracked window strip dynamic materials for emissive pulse */
	UPROPERTY()
	TArray<UMaterialInstanceDynamic*> WindowMaterials;

	/** Previous stage — for detecting transitions */
	FString PreviousStage;

	/** Forge idle intensity (dimmer) */
	float IdleIntensity = 3000.0f;

	/** Whether zone-level production boost is active (from world consequences) */
	bool bProductionActive = false;

	/** Torch flicker phase accumulator. */
	float FlickerPhase = 0.f;

	// -------------------------------------------------------
	// Materialization Burst State
	// -------------------------------------------------------

	/** True while the burst animation is playing */
	bool bMaterializationBurstActive = false;

	/** Elapsed time into the burst animation */
	float BurstElapsed = 0.0f;

	/** Total duration of the burst effect */
	float BurstDuration = 2.0f;

	/** Original pedestal scale (restored after burst) */
	FVector OriginalPedestalScale = FVector(1.2f, 1.2f, 0.15f);
};
