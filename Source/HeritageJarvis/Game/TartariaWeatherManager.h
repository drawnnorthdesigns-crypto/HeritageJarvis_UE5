#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TartariaWeatherManager.generated.h"

class UExponentialHeightFogComponent;
class UNiagaraComponent;
class UPointLightComponent;

/**
 * ATartariaWeatherManager — Unified atmosphere coordinator.
 * Manages fog, weather particles (rain, dust), and clamps post-process
 * values from multiple sources (biome, consequence, day/night) to
 * prevent blackout/whiteout.
 *
 * Place one in the level. The WorldSubsystem sets weather state.
 */
UENUM(BlueprintType)
enum class ETartariaWeather : uint8
{
	Clear       UMETA(DisplayName = "Clear"),
	Overcast    UMETA(DisplayName = "Overcast"),
	Rain        UMETA(DisplayName = "Rain"),
	DustStorm   UMETA(DisplayName = "Dust Storm"),
	AetherMist  UMETA(DisplayName = "Aether Mist"),
};

UCLASS()
class HERITAGEJARVIS_API ATartariaWeatherManager : public AActor
{
	GENERATED_BODY()

public:
	ATartariaWeatherManager();

	virtual void Tick(float DeltaTime) override;

	/** Set the current weather state. Transitions smoothly. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Weather")
	void SetWeather(ETartariaWeather NewWeather);

	/** Get the current weather. */
	UFUNCTION(BlueprintPure, Category = "Tartaria|Weather")
	ETartariaWeather GetCurrentWeather() const { return CurrentWeather; }

	/**
	 * Clamp and coordinate post-process values from multiple sources.
	 * Call this after biome/consequence/day-night systems have set their values.
	 * Ensures fog density stays in [MinFog, MaxFog] and exposure stays sane.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Weather")
	void ClampPostProcessValues();

	/** Fog density limits to prevent blackout/whiteout. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Weather")
	float MinFogDensity = 0.002f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Weather")
	float MaxFogDensity = 0.08f;

	/** Fog component managed by this actor. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Weather")
	UExponentialHeightFogComponent* FogComponent;

	/** Lightning flash light (for rain storms). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Weather")
	UPointLightComponent* LightningLight;

private:
	ETartariaWeather CurrentWeather = ETartariaWeather::Clear;
	ETartariaWeather TargetWeather = ETartariaWeather::Clear;

	/** Transition progress 0-1. */
	float WeatherTransition = 1.f;

	/** Lightning timer for storm effects. */
	float LightningTimer = 0.f;
	float NextLightningTime = 8.f;

	/** Apply weather parameters based on current state. */
	void ApplyWeatherState(float Alpha);
};
