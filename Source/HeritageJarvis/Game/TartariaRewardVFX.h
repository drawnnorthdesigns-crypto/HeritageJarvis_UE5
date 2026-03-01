#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TartariaRewardVFX.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

/**
 * Reward VFX event types.
 */
UENUM(BlueprintType)
enum class ERewardVFXType : uint8
{
	BuildingUpgrade   UMETA(DisplayName = "Building Upgrade"),
	CombatVictory     UMETA(DisplayName = "Combat Victory"),
	QuestComplete     UMETA(DisplayName = "Quest Complete"),
	LevelUp           UMETA(DisplayName = "Level Up"),
	Materialization   UMETA(DisplayName = "CAD Materialization"),
};

/**
 * ATartariaRewardVFX — Transient celebration VFX spawned at key moments.
 *
 * Spawned at a world location, plays a short compound-primitive animation,
 * then self-destructs. Different VFX types produce different effects:
 *
 *   BuildingUpgrade: Golden shockwave ring + rising sparkle particles
 *   CombatVictory:   Shattering cubes outward + spiraling loot orbs
 *   QuestComplete:   Ascending pillar of light + expanding halo
 *   LevelUp:         Concentric expanding rings + upward beam
 *   Materialization: Converging particle swirl + flash
 *
 * All effects are compound-primitives (no Niagara assets required).
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaRewardVFX : public AActor
{
	GENERATED_BODY()

public:
	ATartariaRewardVFX();
	virtual void Tick(float DeltaTime) override;

	/** Initialize and play the VFX. Self-destructs after duration. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|VFX")
	void PlayEffect(ERewardVFXType Type, float Duration = 2.0f);

	/** Static helper: spawn and play at a world location. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|VFX",
	          meta = (WorldContext = "WorldContextObject"))
	static ATartariaRewardVFX* SpawnRewardVFX(const UObject* WorldContextObject,
		FVector Location, ERewardVFXType Type, float Duration = 2.0f);

private:
	/** Build shockwave ring particles. */
	void BuildShockwave();

	/** Build shatter cube particles. */
	void BuildShatter();

	/** Build ascending pillar/halo. */
	void BuildPillar();

	/** Build converging swirl. */
	void BuildSwirl();

	/** Animate based on current type. */
	void AnimateEffect(float DeltaTime);

	UPROPERTY(VisibleAnywhere)
	USceneComponent* VFXRoot;

	/** Central flash light. */
	UPROPERTY(VisibleAnywhere)
	UPointLightComponent* FlashLight;

	/** Particle meshes (varies by effect type). */
	UPROPERTY()
	TArray<UStaticMeshComponent*> ParticleMeshes;

	/** Per-particle velocities. */
	TArray<FVector> ParticleVelocities;

	ERewardVFXType EffectType = ERewardVFXType::BuildingUpgrade;
	float EffectDuration = 2.0f;
	float ElapsedTime = 0.f;
	bool bPlaying = false;
};
