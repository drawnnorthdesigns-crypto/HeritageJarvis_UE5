#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HJInteractable.h"
#include "TartariaTypes.h"
#include "TartariaResourceNode.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

/**
 * ATartariaResourceNode — Harvestable resource deposit.
 * OnInteract: calls /api/game/harvest, triggers visual feedback, reduces yield.
 * Depleted nodes respawn after RespawnTime seconds.
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaResourceNode : public AActor, public IHJInteractable
{
	GENERATED_BODY()

public:
	ATartariaResourceNode();

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Resource")
	ETartariaResourceType ResourceType = ETartariaResourceType::Iron;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Resource")
	int32 MaxYield = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Resource")
	int32 RemainingYield = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Resource")
	float RespawnTime = 120.f;

	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Resource")
	bool bDepleted = false;

	// -------------------------------------------------------
	// Components
	// -------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Resource")
	UStaticMeshComponent* ResourceMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Resource")
	UPointLightComponent* GlowLight;

	// -------------------------------------------------------
	// Blueprint events
	// -------------------------------------------------------

	UFUNCTION(BlueprintImplementableEvent, Category = "Tartaria|Resource")
	void OnHarvested(int32 AmountGathered, int32 Remaining);

	UFUNCTION(BlueprintImplementableEvent, Category = "Tartaria|Resource")
	void OnDepleted();

	UFUNCTION(BlueprintImplementableEvent, Category = "Tartaria|Resource")
	void OnRespawned();

	/** Spawn floating orbs that rise from the node when harvested. */
	void SpawnHarvestEffect();

private:
	void Respawn();
	void SendHarvestRequest(APlayerController* Interactor);

	float RespawnTimer = 0.f;
};
