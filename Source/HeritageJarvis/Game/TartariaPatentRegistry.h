#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HJInteractable.h"
#include "TartariaPatentRegistry.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

/**
 * ATartariaPatentRegistry — Holographic patent display near the Scriptorium.
 *
 * Shows completed engineering projects as floating holographic "patent plaques"
 * made from compound primitives. Each plaque is a thin emissive slab that
 * hovers and rotates slowly. The number of visible plaques scales with
 * the player's completed project count from the Python backend.
 *
 * Interact to open the project archive in the CEF dashboard.
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaPatentRegistry : public AActor, public IHJInteractable
{
	GENERATED_BODY()

public:
	ATartariaPatentRegistry();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// IHJInteractable
	virtual void OnInteract_Implementation(APlayerController* Interactor) override;
	virtual FString GetInteractPrompt_Implementation() const override;

	/** Update the number of visible patent plaques. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Economy")
	void UpdatePatentCount(int32 NewCount);

	/** Maximum plaques that can be displayed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Economy")
	int32 MaxPlaques = 12;

	/** Current visible plaque count. */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Economy")
	int32 CurrentPlaques = 0;

	// Components

	/** Central pedestal. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Economy")
	UStaticMeshComponent* PedestalMesh;

	/** Holographic projector glow. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Economy")
	UPointLightComponent* HoloGlow;

private:
	/** Patent plaque meshes (created at BeginPlay). */
	UPROPERTY()
	TArray<UStaticMeshComponent*> PlaqueMeshes;

	/** Animation time accumulator. */
	float AnimTime = 0.f;

	/** Build the plaque meshes. */
	void BuildPlaques();
};
