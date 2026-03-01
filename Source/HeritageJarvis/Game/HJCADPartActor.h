#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HJInteractable.h"
#include "HJCADPartActor.generated.h"

class UProceduralMeshComponent;
class UHJMeshLoader;
class UHJNaniteConverter;
class UPointLightComponent;

/**
 * AHJCADPartActor — Spawnable actor that loads CAD-generated mesh from the
 * Flask pipeline and places it in the Tartaria game world.
 *
 * This is the bridge between the engineering pipeline (CadQuery → STL → mesh API)
 * and the UE5 game world. When spawned with a project ID and component name,
 * it uses HJMeshLoader to fetch the optimized mesh and renders it in-world.
 *
 * Features:
 *   - Auto-loads mesh from Flask /api/mesh/ on BeginPlay
 *   - Interactable (IHJInteractable) — opens the 3D viewer for this model
 *   - Materialization VFX: starts invisible, fades in with golden particle burst
 *   - Scale: auto-fits to a configurable world-space bounding size
 *
 * Spawn from code:
 *   AHJCADPartActor* Part = World->SpawnActor<AHJCADPartActor>(...);
 *   Part->ProjectId = "abc123";
 *   Part->ComponentName = "bracket";
 *   Part->LoadFromPipeline();
 */
UCLASS()
class HERITAGEJARVIS_API AHJCADPartActor : public AActor, public IHJInteractable
{
	GENERATED_BODY()

public:
	AHJCADPartActor();

	virtual void BeginPlay() override;

	// ── IHJInteractable ─────────────────────────────────────
	virtual void OnInteract_Implementation(APlayerController* Interactor) override;
	virtual FString GetInteractPrompt_Implementation() const override;

	// ── Configuration ───────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CAD|Config")
	FString ProjectId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CAD|Config")
	FString ComponentName;

	/** Maximum world-space size for the part (cm). Mesh auto-scales to fit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CAD|Config")
	float MaxWorldSize = 200.f;

	/** Whether to auto-load on BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CAD|Config")
	bool bAutoLoad = true;

	/** Display name shown on interact prompt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CAD|Config")
	FString DisplayName = TEXT("CAD Part");

	// ── Components ──────────────────────────────────────────
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CAD")
	UProceduralMeshComponent* PartMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CAD")
	UHJMeshLoader* MeshLoader;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CAD")
	UHJNaniteConverter* NaniteConverter;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CAD")
	UPointLightComponent* PartLight;

	// ── Status ──────────────────────────────────────────────
	UPROPERTY(BlueprintReadOnly, Category = "CAD|Status")
	bool bMeshLoaded = false;

	UPROPERTY(BlueprintReadOnly, Category = "CAD|Status")
	int32 TriangleCount = 0;

	/** True if mesh was converted to Nanite StaticMesh. */
	UPROPERTY(BlueprintReadOnly, Category = "CAD|Status")
	bool bNaniteConverted = false;

	// ── Blueprint Events ────────────────────────────────────
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCADPartLoaded, bool, bSuccess);

	UPROPERTY(BlueprintAssignable, Category = "CAD|Events")
	FOnCADPartLoaded OnPartLoaded;

	// ── Public Methods ──────────────────────────────────────
	/** Trigger mesh load from the Flask pipeline. */
	UFUNCTION(BlueprintCallable, Category = "CAD")
	void LoadFromPipeline();

private:
	UFUNCTION()
	void OnMeshLoadComplete(bool bSuccess, const FString& StatusMessage);
};
