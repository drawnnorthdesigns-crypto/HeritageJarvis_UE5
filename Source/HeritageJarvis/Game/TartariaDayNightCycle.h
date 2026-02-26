#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TartariaDayNightCycle.generated.h"

class ADirectionalLight;

/**
 * ATartariaDayNightCycle — Controls sun rotation and sky lighting.
 * Place one in the TartariaWorld level, assign the DirectionalLight.
 *
 * Sun pitch: -90 (midnight) -> 0 (sunrise at 6h) -> 90 (noon) -> 0 (sunset) -> -90
 * DayLengthSeconds controls real-time length of one full game day.
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaDayNightCycle : public AActor
{
	GENERATED_BODY()

public:
	ATartariaDayNightCycle();

	virtual void Tick(float DeltaTime) override;

	// -------------------------------------------------------
	// Configuration
	// -------------------------------------------------------

	/** Reference to the directional light acting as the sun. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|DayNight")
	ADirectionalLight* SunLight;

	/** Real-time seconds for one full game day (0-24). Default 600s = 10 min. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|DayNight")
	float DayLengthSeconds = 600.f;

	/** Current time of day (0-24 range). Advances automatically. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|DayNight")
	float TimeOfDay = 8.0f;

	/** Sun intensity at noon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|DayNight")
	float MaxSunIntensity = 10.f;

	/** Sun intensity at midnight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|DayNight")
	float MinSunIntensity = 0.1f;

	// -------------------------------------------------------
	// Read-only state
	// -------------------------------------------------------

	/** Is it currently nighttime (between 20:00 and 5:00)? */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|DayNight")
	bool bIsNight = false;

	/** Normalized sun height (0 at horizon, 1 at zenith). */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|DayNight")
	float SunHeightNormalized = 0.5f;

private:
	void UpdateSunPosition();
};
