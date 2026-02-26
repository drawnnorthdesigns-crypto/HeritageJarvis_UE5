#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HJInteractable.h"
#include "TartariaTypes.h"
#include "TartariaForgeBuilding.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

/**
 * ATartariaForgeBuilding — Interactable building actor in the Tartaria world.
 * Types: Forge, Scriptorium, Treasury, Barracks, Lab.
 * OnInteract: opens building-specific UI or navigates CEF to building route.
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaForgeBuilding : public AActor, public IHJInteractable
{
	GENERATED_BODY()

public:
	ATartariaForgeBuilding();

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
	// Components
	// -------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Building")
	UStaticMeshComponent* BuildingMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Building")
	UPointLightComponent* BuildingLight;

	// -------------------------------------------------------
	// Blueprint events
	// -------------------------------------------------------

	UFUNCTION(BlueprintImplementableEvent, Category = "Tartaria|Building")
	void OnBuildingInteracted(APlayerController* Interactor);
};
