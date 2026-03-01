#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TartariaBiomeParticles.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class UParticleSystemComponent;

/**
 * ATartariaBiomeParticles — Per-biome atmospheric particle emitter.
 *
 * Spawns Niagara or legacy Cascade particles with biome-specific parameters.
 * Dust motes (Clearinghouse), floating pages (Scriptorium), dark wisps
 * (Monolith Ward), ember sparks (Forge District), void tendrils (Void Reach).
 *
 * If a NiagaraSystem asset is assigned, uses UNiagaraComponent.
 * Otherwise falls back to a UParticleSystemComponent with basic dust material.
 * Parameters (color, spawn rate, velocity, size) are overridden per-biome
 * via ConfigureForBiome().
 *
 * IMPORTANT: All particle systems MUST use LOCAL SPACE (not World) to survive
 * origin rebasing during interplanetary transit.
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaBiomeParticles : public AActor
{
	GENERATED_BODY()

public:
	ATartariaBiomeParticles();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// -------------------------------------------------------
	// Configuration
	// -------------------------------------------------------

	/** Biome identifier (matches TartariaBiomeVolume::BiomeKey). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Particles")
	FString BiomeKey;

	/** Niagara system asset (optional — assign in editor). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Particles")
	UNiagaraSystem* NiagaraSystemAsset;

	/** Particle color tint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Particles")
	FLinearColor ParticleColor = FLinearColor(0.8f, 0.7f, 0.5f, 0.4f);

	/** Spawn rate (particles per second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Particles")
	float SpawnRate = 15.f;

	/** Spawn volume radius (particles spawn within this radius). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Particles")
	float SpawnRadius = 5000.f;

	/** Particle base scale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Particles")
	float ParticleScale = 1.0f;

	/** Vertical drift speed (positive = upward). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Particles")
	float DriftSpeed = 50.f;

	/** Particle lifetime in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Particles")
	float ParticleLifetime = 8.f;

	// -------------------------------------------------------
	// Components
	// -------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Particles")
	UNiagaraComponent* NiagaraComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Particles")
	USceneComponent* SceneRoot;

	// -------------------------------------------------------
	// Runtime
	// -------------------------------------------------------

	/**
	 * Apply biome-specific particle parameters based on BiomeKey.
	 * Call from WorldPopulator AFTER setting BiomeKey.
	 */
	void ConfigureForBiome();

	/** Activate particle emission. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Particles")
	void ActivateParticles();

	/** Deactivate particle emission. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Particles")
	void DeactivateParticles();

private:
	/** Track active state. */
	bool bParticlesActive = false;

	/** Follow player if within biome (set in Tick). */
	void FollowPlayer();
};
