#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TartariaAmbientSound.generated.h"

class UAudioComponent;

/**
 * ATartariaAmbientSound — Ambient sound emitter for Tartaria biome zones.
 *
 * Each instance wraps a UAudioComponent with biome-appropriate attenuation,
 * volume, and pitch settings. Assign a USoundBase asset per biome via
 * BiomeSoundCue (in editor or code). If no asset is assigned, the component
 * stays silent.
 *
 * Multiple instances can be placed within a biome for spatial variety.
 * Volume automatically attenuates over AttenuationRadius.
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaAmbientSound : public AActor
{
	GENERATED_BODY()

public:
	ATartariaAmbientSound();

	virtual void BeginPlay() override;

	// -------------------------------------------------------
	// Configuration
	// -------------------------------------------------------

	/** The biome this sound belongs to (for populator reference). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Sound")
	FString BiomeKey;

	/** Sound asset to play. Assign a looping SoundWave/Cue per biome. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Sound")
	USoundBase* BiomeSoundCue;

	/** Base volume multiplier (0.0 - 1.0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Sound", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Volume = 0.5f;

	/** Pitch multiplier (0.5 = octave down, 2.0 = octave up). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Sound", meta = (ClampMin = "0.2", ClampMax = "3.0"))
	float PitchMultiplier = 1.0f;

	/** Max distance (UU) at which the sound is audible. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Sound")
	float AttenuationRadius = 50000.f;

	/** Fade-in time on BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Sound")
	float FadeInDuration = 3.0f;

	// -------------------------------------------------------
	// Components
	// -------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Sound")
	UAudioComponent* AudioComp;

	// -------------------------------------------------------
	// Runtime control
	// -------------------------------------------------------

	/** Start playback (fade in). */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Sound")
	void StartAmbient();

	/** Stop playback (fade out). */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Sound")
	void StopAmbient(float FadeOutDuration = 2.0f);

	/**
	 * Configure attenuation and parameters based on BiomeKey.
	 * Called from WorldPopulator after setting BiomeKey.
	 */
	void ConfigureForBiome();
};
