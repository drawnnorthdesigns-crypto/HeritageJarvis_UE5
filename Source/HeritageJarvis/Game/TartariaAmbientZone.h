#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TartariaTypes.h"
#include "TartariaAmbientZone.generated.h"

class UAudioComponent;

/**
 * ATartariaAmbientZone -- Per-biome ambient audio actor.
 * Placed in each biome zone to provide environmental soundscapes.
 * Uses engine-provided sound assets where available, or silence
 * with eventual procedural synth integration.
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaAmbientZone : public AActor
{
	GENERATED_BODY()

public:
	ATartariaAmbientZone();

	virtual void Tick(float DeltaTime) override;

	// -------------------------------------------------------
	// Properties
	// -------------------------------------------------------

	/** Which biome this zone represents. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Audio")
	ETartariaBiome BiomeType = ETartariaBiome::Clearinghouse;

	/** Master volume for this zone (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Audio")
	float MasterVolume = 0.5f;

	/** Range within which audio is audible. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Audio")
	float AudibleRadius = 5000.f;

	// -------------------------------------------------------
	// Components
	// -------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Audio")
	UAudioComponent* AmbientAudio;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Audio")
	USceneComponent* AudioRoot;

private:
	/** Adjust volume based on player distance (spatial falloff). */
	void UpdateSpatialVolume(float DeltaTime);

	float CurrentVolume = 0.f;
};
