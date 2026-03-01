#pragma once

#include "CoreMinimal.h"
#include "TartariaTypes.h"
#include "TartariaSoundManager.generated.h"

class USoundBase;
class UAudioComponent;

/**
 * UTartariaSoundManager -- Centralized interaction sound playback.
 * Provides static methods for common game sounds.
 * Uses engine tone sounds as placeholders (no imported assets required).
 * Called from Character, NPC, ForgeBuilding, etc.
 *
 * Material-Specific Impact Audio (Task #205):
 * Surfaces are classified by ETartariaPhysicalMaterial and each carries an
 * FMaterialAudioProfile that drives pitch, decay, resonance, and volume
 * of impact/footstep/combat sounds. Profiles are initialized once and
 * queried via GetMaterialProfile().
 */
UCLASS()
class HERITAGEJARVIS_API UTartariaSoundManager : public UObject
{
	GENERATED_BODY()

public:
	/** Play a UI click sound (short blip). */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Sound", meta = (WorldContext = "WorldContextObject"))
	static void PlayUIClick(UObject* WorldContextObject);

	/** Play a success chime (rising tone). */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Sound", meta = (WorldContext = "WorldContextObject"))
	static void PlaySuccess(UObject* WorldContextObject);

	/** Play an error buzz. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Sound", meta = (WorldContext = "WorldContextObject"))
	static void PlayError(UObject* WorldContextObject);

	/** Play a harvest/collect sound. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Sound", meta = (WorldContext = "WorldContextObject"))
	static void PlayHarvest(UObject* WorldContextObject);

	/** Play a generic combat impact sound (legacy, no material info). */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Sound", meta = (WorldContext = "WorldContextObject"))
	static void PlayCombatHit(UObject* WorldContextObject);

	/** Play an NPC greeting chime. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Sound", meta = (WorldContext = "WorldContextObject"))
	static void PlayNPCGreeting(UObject* WorldContextObject);

	/** Play a menu open/close sound. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Sound", meta = (WorldContext = "WorldContextObject"))
	static void PlayMenuToggle(UObject* WorldContextObject);

	/** Play a forge seal / save chime. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Sound", meta = (WorldContext = "WorldContextObject"))
	static void PlayForgeSeal(UObject* WorldContextObject);

	/** Play a footstep sound (legacy, no material info). */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Sound", meta = (WorldContext = "WorldContextObject"))
	static void PlayFootstep(UObject* WorldContextObject, bool bSprinting);

	/** Play a dodge roll whoosh. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Sound", meta = (WorldContext = "WorldContextObject"))
	static void PlayDodge(UObject* WorldContextObject);

	/** Play a level-up fanfare. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Sound", meta = (WorldContext = "WorldContextObject"))
	static void PlayLevelUp(UObject* WorldContextObject);

	// =============================================================
	// Biome Ambient Audio (Task #215)
	// =============================================================

	/**
	 * Get the ambient audio profile for a biome type.
	 * Returns a const reference to the statically-initialized profile.
	 */
	static const FBiomeAudioProfile& GetBiomeProfile(ETartariaBiome Biome);

	/**
	 * Convert a BiomeKey string (e.g. "FORGE_DISTRICT") to ETartariaBiome enum.
	 * Returns Clearinghouse as fallback for unrecognized keys.
	 */
	static ETartariaBiome BiomeFromKey(const FString& BiomeKey);

	/**
	 * Configure a UAudioComponent for biome ambient playback.
	 * Sets pitch, volume, and looping based on the biome's audio profile.
	 * The component should use a looping engine sound (loaded automatically).
	 *
	 * @param AudioComp  The audio component to configure (owned by caller).
	 * @param Biome      The biome type to configure for.
	 */
	static void ConfigureBiomeAmbient(UAudioComponent* AudioComp, ETartariaBiome Biome);

	/**
	 * Apply rhythmic volume modulation to a biome ambient audio component.
	 * Call this from Tick() to produce LFO-like oscillation on the ambient loop.
	 *
	 * @param AudioComp       The ambient audio component to modulate.
	 * @param Biome           The biome type (for profile lookup).
	 * @param TimeAccumulator Running time accumulator (caller should += DeltaTime).
	 */
	static void TickBiomeAmbient(UAudioComponent* AudioComp, ETartariaBiome Biome, float TimeAccumulator);

	// =============================================================
	// Material-Specific Impact Audio (Task #205)
	// =============================================================

	/**
	 * Get the audio profile for a physical material type.
	 * Returns a const reference to the statically-initialized profile.
	 */
	static const FMaterialAudioProfile& GetMaterialProfile(ETartariaPhysicalMaterial Material);

	/**
	 * Play a material-specific impact sound at a world location.
	 * Pitch is randomized around BasePitch, volume scales with Intensity,
	 * and resonance controls sustain/reverb feel via layered playback.
	 *
	 * @param WorldContextObject  Any UObject with a valid world context.
	 * @param Material            The surface material being impacted.
	 * @param Location            World-space location for 3D attenuation.
	 * @param Intensity           Normalized impact force (0-1). Scales volume.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Sound", meta = (WorldContext = "WorldContextObject"))
	static void PlayMaterialImpact(UObject* WorldContextObject,
		ETartariaPhysicalMaterial Material, FVector Location, float Intensity = 0.5f);

	/**
	 * Play a material-aware footstep at the character's location.
	 * Uses the floor material profile for pitch/volume characteristics.
	 *
	 * @param WorldContextObject  Any UObject with a valid world context.
	 * @param Material            The floor surface material under the player.
	 * @param Location            Foot location in world space.
	 * @param SpeedFraction       Current speed / max speed (0-1). Affects volume.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Sound", meta = (WorldContext = "WorldContextObject"))
	static void PlayMaterialFootstep(UObject* WorldContextObject,
		ETartariaPhysicalMaterial Material, FVector Location, float SpeedFraction = 0.5f);

	/**
	 * Play a material-aware combat hit sound.
	 * Blends the weapon material and target material audio profiles to
	 * produce a layered impact: weapon ring + target crack/thud.
	 *
	 * @param WorldContextObject  Any UObject with a valid world context.
	 * @param WeaponMaterial      Material of the attacking weapon.
	 * @param TargetMaterial      Material of the target surface/armor.
	 * @param Location            World-space hit point.
	 * @param Damage              Raw damage dealt (higher = louder + deeper).
	 */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Sound", meta = (WorldContext = "WorldContextObject"))
	static void PlayMaterialCombatHit(UObject* WorldContextObject,
		ETartariaPhysicalMaterial WeaponMaterial, ETartariaPhysicalMaterial TargetMaterial,
		FVector Location, float Damage = 15.f);

private:
	/** Play a built-in engine sound by path, with volume and pitch. */
	static void PlayEngineSound(UObject* WorldContextObject, float Volume = 1.0f, float Pitch = 1.0f);

	/** Play a built-in engine sound at a 3D location, with volume and pitch. */
	static void PlayEngineSoundAtLocation(UObject* WorldContextObject,
		FVector Location, float Volume = 1.0f, float Pitch = 1.0f);

	/**
	 * Initialize the static material profile table (called once lazily).
	 * Profiles map ETartariaPhysicalMaterial -> FMaterialAudioProfile.
	 */
	static void EnsureProfilesInitialized();

	/** Static profile array indexed by ETartariaPhysicalMaterial. */
	static TArray<FMaterialAudioProfile> MaterialProfiles;

	/** Whether MaterialProfiles has been populated. */
	static bool bProfilesInitialized;

	// ── Biome Ambient Audio (Task #215) ──────────────────────────

	/** Initialize the static biome profile table (called once lazily). */
	static void EnsureBiomeProfilesInitialized();

	/** Static profile array indexed by ETartariaBiome. */
	static TArray<FBiomeAudioProfile> BiomeProfiles;

	/** Whether BiomeProfiles has been populated. */
	static bool bBiomeProfilesInitialized;
};
