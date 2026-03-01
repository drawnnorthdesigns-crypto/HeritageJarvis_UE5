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

private:
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
};
