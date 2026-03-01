#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TartariaTypes.h"
#include "TartariaBiomeVolume.generated.h"

class USphereComponent;
class UPostProcessComponent;
class UAudioComponent;
class UParticleSystemComponent;
class UExponentialHeightFogComponent;

/**
 * ATartariaBiomeVolume — Placed in level to define biome zone boundaries.
 * Each biome has unique post-processing atmosphere and collision detection.
 * On zone entry, calls /api/game/threat/check to determine if an encounter triggers.
 *
 * Post-process settings are configured per-biome via ConfigureBiomeAtmosphere(),
 * providing distinct color grading, bloom, vignette, and fog tint per zone.
 *
 * World Consequence Integration (Task #202):
 * Periodically polls /api/game/world-consequences/{BiomeKey} to apply
 * dynamic visual modifiers (tint, fog density, saturation) based on
 * permanent world state changes driven by player actions.
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaBiomeVolume : public AActor
{
	GENERATED_BODY()

public:
	ATartariaBiomeVolume();

	/** Biome identifier matching backend keys. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Biome")
	FString BiomeKey = TEXT("CLEARINGHOUSE");

	/** Radius of the biome zone in UE units (100 UU = 1m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Biome")
	float ZoneRadius = 150000.f;

	/** Difficulty level (1-5). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Biome")
	int32 Difficulty = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Biome")
	USphereComponent* ZoneBounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Biome")
	UPostProcessComponent* BiomePostProcess;

	/** Ambient audio component for zone atmosphere */
	UPROPERTY(VisibleAnywhere, Category = "Atmosphere")
	UAudioComponent* AmbientAudio;

	/** Particle system for zone ambiance (dust, embers, void sparks) */
	UPROPERTY(VisibleAnywhere, Category = "Atmosphere")
	UParticleSystemComponent* AmbientParticles;

	/**
	 * Apply biome-specific post-process settings based on BiomeKey.
	 * Call this AFTER setting BiomeKey (WorldPopulator does this after spawn).
	 */
	void ConfigureBiomeAtmosphere();

	/**
	 * Enhanced per-biome post-process configuration (Task #208).
	 * Applies SceneColorTint, bloom, vignette, film grain, chromatic aberration,
	 * and color grading per biome zone to create strong visual identity from distance.
	 * Called from ConfigureBiomeAtmosphere() after base settings.
	 */
	void ConfigureBiomePostProcess();

	// -------------------------------------------------------
	// Threat detection (Phase 2)
	// -------------------------------------------------------

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnThreatDetected,
		const FTartariaThreatInfo&, Threat, ATartariaBiomeVolume*, Volume);

	/** Fires when a threat is detected on zone entry. */
	UPROPERTY(BlueprintAssignable, Category = "Tartaria|Combat")
	FOnThreatDetected OnThreatDetected;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnZoneChanged,
		const FString&, NewBiome, int32, NewDifficulty);

	/** Fires when player enters this zone (always, regardless of threat). */
	UPROPERTY(BlueprintAssignable, Category = "Tartaria|Biome")
	FOnZoneChanged OnZoneEntered;

	// -------------------------------------------------------
	// World Consequence Visual Modifiers (Task #202)
	// -------------------------------------------------------

	/** Post-process component dedicated to consequence visual overlays. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Consequences")
	UPostProcessComponent* ConsequencePostProcess;

	/** Exponential height fog for consequence-driven fog density changes. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Consequences")
	UExponentialHeightFogComponent* ConsequenceFog;

	/** How often (in seconds) to poll the Flask backend for consequence modifiers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Consequences")
	float ConsequencePollIntervalSec = 30.f;

	/** Speed at which current visuals lerp toward target (per second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Consequences")
	float ConsequenceLerpSpeed = 0.5f;

	/** Zone health score from backend (0=desolation, 100=prosperity). */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Consequences")
	int32 ZoneHealth = 50;

	/** Whether the corruption flag is active for this zone. */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Consequences")
	bool bCorruptionActive = false;

	/** Fires when consequence modifiers update (for UI/gameplay hooks). */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnConsequenceModifiersUpdated,
		int32, Health, bool, bCorrupted, const FString&, Description);

	UPROPERTY(BlueprintAssignable, Category = "Tartaria|Consequences")
	FOnConsequenceModifiersUpdated OnConsequenceModifiersUpdated;

	/** Fetch consequence modifiers from the Flask backend. */
	void FetchConsequenceModifiers();

	/** Smoothly apply consequence modifiers to post-process and fog. */
	void ApplyConsequenceModifiers(float DeltaTime);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	void OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent,
	                         AActor* OtherActor, UPrimitiveComponent* OtherComp,
	                         int32 OtherBodyIndex, bool bFromSweep,
	                         const FHitResult& SweepResult);

	UFUNCTION()
	void OnZoneEndOverlap(UPrimitiveComponent* OverlappedComponent,
	                       AActor* OtherActor, UPrimitiveComponent* OtherComp,
	                       int32 OtherBodyIndex);

private:
	void OnThreatCheckResponse(bool bSuccess, const FString& Body);
	void OnConsequenceResponse(bool bSuccess, const FString& Body);

	/** Cooldown to prevent rapid re-triggering. */
	float LastThreatCheckTime = -999.f;
	static constexpr float ThreatCheckCooldownSec = 10.f;

	// ── Consequence visual state ──────────────────────────────
	/** Timer accumulator for consequence polling. */
	float ConsequencePollTimer = 0.f;

	/** Whether a consequence fetch is in-flight (prevent overlapping requests). */
	bool bConsequenceFetchInFlight = false;

	/** Whether the player is currently inside this volume. */
	bool bPlayerInside = false;

	/** Target values from the latest backend response. */
	FLinearColor TargetTint = FLinearColor(1.f, 1.f, 1.f, 1.f);
	float TargetFogDensity = 0.02f;
	float TargetSaturation = 1.0f;

	/** Current interpolated values applied to post-process/fog. */
	FLinearColor CurrentTint = FLinearColor(1.f, 1.f, 1.f, 1.f);
	float CurrentFogDensity = 0.02f;
	float CurrentSaturation = 1.0f;

	/** Corruption pulsing state (sinusoidal fog oscillation). */
	float CorruptionPulseAccumulator = 0.f;

	/** Cached description from last backend response. */
	FString LastDescription;
};
