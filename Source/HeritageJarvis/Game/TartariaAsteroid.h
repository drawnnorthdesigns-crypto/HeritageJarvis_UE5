#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HJInteractable.h"
#include "TartariaAsteroid.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

/** Asteroid type — determines colour, glow, and resource profile. */
UENUM(BlueprintType)
enum class EAsteroidType : uint8
{
	Metallic  UMETA(DisplayName = "Metallic"),
	Icy       UMETA(DisplayName = "Icy"),
	Rocky     UMETA(DisplayName = "Rocky"),
};

/**
 * ATartariaAsteroid — Mineable asteroid in the solar system.
 *
 * Implements IHJInteractable.  OnInteract calls POST /api/game/mining/extract.
 * Visual glow dims as yield depletes.  Asteroid slowly tumbles in Tick.
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaAsteroid : public AActor, public IHJInteractable
{
	GENERATED_BODY()

public:
	ATartariaAsteroid();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// -------------------------------------------------------
	// IHJInteractable
	// -------------------------------------------------------

	virtual void OnInteract_Implementation(APlayerController* Interactor) override;
	virtual FString GetInteractPrompt_Implementation() const override;

	// -------------------------------------------------------
	// Configurable properties
	// -------------------------------------------------------

	/** Unique backend id, e.g. "jupiter_ast_2". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Mining")
	FString AsteroidId;

	/** Celestial body this asteroid belongs to (e.g. "jupiter"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Mining")
	FString BodyId;

	/** Asteroid material class (drives glow colour and mesh scale). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Mining")
	EAsteroidType AsteroidType = EAsteroidType::Rocky;

	/** Current extractable yield units (mirrors backend). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Mining")
	float CurrentYield = 100.f;

	/** Maximum yield at spawn (used to compute depletion ratio). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Mining")
	float MaxYield = 100.f;

	/** Units requested per player interaction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Mining")
	float ExtractAmount = 10.f;

	// -------------------------------------------------------
	// Components
	// -------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Mining")
	UStaticMeshComponent* AsteroidMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Mining")
	UPointLightComponent* ResourceGlow;

	// -------------------------------------------------------
	// Blueprint events
	// -------------------------------------------------------

	UFUNCTION(BlueprintImplementableEvent, Category = "Tartaria|Mining")
	void OnMineSuccess(float AmountMined, float Remaining);

	UFUNCTION(BlueprintImplementableEvent, Category = "Tartaria|Mining")
	void OnMineFailed(const FString& Reason);

	UFUNCTION(BlueprintImplementableEvent, Category = "Tartaria|Mining")
	void OnFullyDepleted();

	// -------------------------------------------------------
	// C++ helpers (called from backend response callback)
	// -------------------------------------------------------

	/** Called with backend result; updates visuals and fires Blueprint events. */
	void OnMineResult(bool bSuccess, float AmountMined, float Remaining, const FString& Message);

	/** Recalculate glow intensity/colour based on CurrentYield / MaxYield. */
	void UpdateVisuals();

	/** Spawn floating harvest orbs (same pattern as ATartariaResourceNode). */
	void SpawnHarvestEffect();

private:
	void SendMineRequest();

	/** Accumulated tumble rotation per axis (degrees/sec). */
	FVector TumbleRate;
};
