#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TartariaDayNightCycle.generated.h"

class ADirectionalLight;
class AExponentialHeightFog;
class USkyLightComponent;

/**
 * ATartariaDayNightCycle — Controls sun rotation, sky lighting, fog, and atmosphere.
 * Place one in the TartariaWorld level, assign the DirectionalLight.
 *
 * Sun pitch: -90 (midnight) -> 0 (sunrise at 6h) -> 90 (noon) -> 0 (sunset) -> -90
 * DayLengthSeconds controls real-time length of one full game day.
 *
 * Also manages:
 *   - SkyLight for ambient fill (prevents pitch-black shadows at night)
 *   - ExponentialHeightFog (spawned at BeginPlay, color/density shifts with time)
 *   - Sun color temperature (warm sunrise/sunset, cool noon, cold moonlight)
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaDayNightCycle : public AActor
{
	GENERATED_BODY()

public:
	ATartariaDayNightCycle();

	virtual void BeginPlay() override;
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
	// Atmosphere components
	// -------------------------------------------------------

	/** Sky light for ambient fill lighting. Owned by this actor. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|DayNight")
	USkyLightComponent* SkyLightComp;

	/** Reference to the spawned fog actor. */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|DayNight")
	AExponentialHeightFog* FogActor;

	// -------------------------------------------------------
	// Read-only state
	// -------------------------------------------------------

	/** Is it currently nighttime (between 20:00 and 5:00)? */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|DayNight")
	bool bIsNight = false;

	/** Normalized sun height (0 at horizon, 1 at zenith). */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|DayNight")
	float SunHeightNormalized = 0.5f;

	// -------------------------------------------------------
	// Star field + Aurora
	// -------------------------------------------------------

	/** Star visibility (0=hidden, 1=full brightness). Updated per tick. */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|DayNight")
	float StarVisibility = 0.f;

	// -------------------------------------------------------
	// Weather System
	// -------------------------------------------------------

	/** Current weather state */
	UPROPERTY(BlueprintReadOnly, Category = "Weather")
	FString CurrentWeather = TEXT("Clear");

	/** Weather transition timer */
	UPROPERTY(BlueprintReadOnly, Category = "Weather")
	float WeatherIntensity = 0.0f;

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void SetWeather(const FString& Weather, float TransitionTime = 5.0f);

	// -------------------------------------------------------
	// Forced Sunrise (Dawn Ceremony)
	// -------------------------------------------------------

	/** Force an accelerated sunrise over Duration seconds. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|DayNight")
	void ForceSunrise(float Duration = 10.f);

private:
	/** True while an accelerated sunrise is in progress. */
	bool bForcingSunrise = false;

	/** Speed (hours per second) to advance time during forced sunrise. */
	float ForceSunriseSpeed = 0.f;

	/** Target time of day for forced sunrise (dawn = 6.0h). */
	float ForceSunriseTarget = 6.0f;
	FString TargetWeather = TEXT("Clear");
	float WeatherTransitionTimer = 0.0f;
	float WeatherTransitionDuration = 5.0f;
	float WeatherChangeCountdown = 120.0f;  // seconds until random weather change

	void UpdateWeather(float DeltaTime);
	float GetWeatherFogMultiplier() const;

	void UpdateSunPosition();
	void UpdateAtmosphere();
	void UpdateStarField();
	void SpawnFogActor();
	void SpawnStarField();

	/** Lerp a color across 3 key-stops (dawn/noon/dusk/night). */
	FLinearColor GetSkyColorForTime() const;
	FLinearColor GetFogColorForTime() const;

	/** Star field meshes (small emissive cubes). */
	UPROPERTY()
	TArray<UStaticMeshComponent*> StarMeshes;

	/** Aurora light beams (colored point lights). */
	UPROPERTY()
	TArray<UPointLightComponent*> AuroraLights;

	/** Aurora animation time. */
	float AuroraTime = 0.f;
};
