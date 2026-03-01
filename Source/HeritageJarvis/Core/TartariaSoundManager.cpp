#include "TartariaSoundManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"

// -------------------------------------------------------
// Static member initialization
// -------------------------------------------------------

TArray<FMaterialAudioProfile> UTartariaSoundManager::MaterialProfiles;
bool UTartariaSoundManager::bProfilesInitialized = false;

TArray<FBiomeAudioProfile> UTartariaSoundManager::BiomeProfiles;
bool UTartariaSoundManager::bBiomeProfilesInitialized = false;

// -------------------------------------------------------
// Material Profile Table
// -------------------------------------------------------

void UTartariaSoundManager::EnsureProfilesInitialized()
{
	if (bProfilesInitialized) return;
	bProfilesInitialized = true;

	// Pre-allocate for all 6 material types
	const int32 NumMaterials = 6;
	MaterialProfiles.SetNum(NumMaterials);

	// Stone: deep thud -- dense, low pitch, short decay, low resonance
	{
		FMaterialAudioProfile& P = MaterialProfiles[(int32)ETartariaPhysicalMaterial::Stone];
		P.BasePitch    = 120.f;
		P.PitchVariance = 0.12f;
		P.Decay        = 0.15f;
		P.Resonance    = 0.2f;
		P.DebugColor   = FLinearColor(0.5f, 0.5f, 0.5f);  // Gray
	}

	// Metal: bright ring -- high pitch, long decay, high resonance
	{
		FMaterialAudioProfile& P = MaterialProfiles[(int32)ETartariaPhysicalMaterial::Metal];
		P.BasePitch    = 800.f;
		P.PitchVariance = 0.08f;
		P.Decay        = 0.4f;
		P.Resonance    = 0.8f;
		P.DebugColor   = FLinearColor(0.8f, 0.8f, 0.9f);  // Silver
	}

	// Wood: dull thud -- mid-low pitch, very short decay, moderate resonance
	{
		FMaterialAudioProfile& P = MaterialProfiles[(int32)ETartariaPhysicalMaterial::Wood];
		P.BasePitch    = 200.f;
		P.PitchVariance = 0.15f;
		P.Decay        = 0.1f;
		P.Resonance    = 0.3f;
		P.DebugColor   = FLinearColor(0.55f, 0.35f, 0.15f);  // Brown
	}

	// Crystal: high chime -- very high pitch, long decay, very high resonance
	{
		FMaterialAudioProfile& P = MaterialProfiles[(int32)ETartariaPhysicalMaterial::Crystal];
		P.BasePitch    = 2000.f;
		P.PitchVariance = 0.05f;
		P.Decay        = 0.6f;
		P.Resonance    = 0.9f;
		P.DebugColor   = FLinearColor(0.4f, 0.8f, 1.0f);  // Cyan
	}

	// Earth: muffled thump -- very low pitch, very short decay, minimal resonance
	{
		FMaterialAudioProfile& P = MaterialProfiles[(int32)ETartariaPhysicalMaterial::Earth];
		P.BasePitch    = 80.f;
		P.PitchVariance = 0.2f;
		P.Decay        = 0.08f;
		P.Resonance    = 0.1f;
		P.DebugColor   = FLinearColor(0.4f, 0.3f, 0.15f);  // Dirt brown
	}

	// Water: splash -- mid pitch, moderate decay, moderate resonance
	{
		FMaterialAudioProfile& P = MaterialProfiles[(int32)ETartariaPhysicalMaterial::Water];
		P.BasePitch    = 400.f;
		P.PitchVariance = 0.18f;
		P.Decay        = 0.3f;
		P.Resonance    = 0.5f;
		P.DebugColor   = FLinearColor(0.1f, 0.3f, 0.8f);  // Blue
	}

	UE_LOG(LogTemp, Log, TEXT("TartariaSoundManager: Material audio profiles initialized (%d materials)"), NumMaterials);
}

const FMaterialAudioProfile& UTartariaSoundManager::GetMaterialProfile(ETartariaPhysicalMaterial Material)
{
	EnsureProfilesInitialized();

	int32 Index = (int32)Material;
	if (Index >= 0 && Index < MaterialProfiles.Num())
	{
		return MaterialProfiles[Index];
	}

	// Fallback to Stone if somehow out of range
	return MaterialProfiles[(int32)ETartariaPhysicalMaterial::Stone];
}

// -------------------------------------------------------
// Core playback helpers
// -------------------------------------------------------

void UTartariaSoundManager::PlayEngineSound(UObject* WorldContextObject, float Volume, float Pitch)
{
	if (!WorldContextObject) return;

	// Try to load a basic engine sound (exists in all UE5 installs)
	static USoundBase* EngineBeep = LoadObject<USoundBase>(nullptr,
		TEXT("/Engine/VREditor/Sounds/UI/Object_PickUp"));

	if (EngineBeep)
	{
		UGameplayStatics::PlaySound2D(WorldContextObject, EngineBeep, Volume, Pitch);
	}
}

void UTartariaSoundManager::PlayEngineSoundAtLocation(UObject* WorldContextObject,
	FVector Location, float Volume, float Pitch)
{
	if (!WorldContextObject) return;

	static USoundBase* EngineBeep = LoadObject<USoundBase>(nullptr,
		TEXT("/Engine/VREditor/Sounds/UI/Object_PickUp"));

	if (EngineBeep)
	{
		UGameplayStatics::PlaySoundAtLocation(WorldContextObject, EngineBeep,
			Location, FRotator::ZeroRotator, Volume, Pitch);
	}
}

// -------------------------------------------------------
// Legacy sound methods (unchanged signatures)
// -------------------------------------------------------

void UTartariaSoundManager::PlayUIClick(UObject* WorldContextObject)
{
	PlayEngineSound(WorldContextObject, 0.3f, 2.0f);  // High pitch, soft
}

void UTartariaSoundManager::PlaySuccess(UObject* WorldContextObject)
{
	PlayEngineSound(WorldContextObject, 0.5f, 1.5f);  // Rising feel
}

void UTartariaSoundManager::PlayError(UObject* WorldContextObject)
{
	PlayEngineSound(WorldContextObject, 0.4f, 0.5f);  // Low, buzzy
}

void UTartariaSoundManager::PlayHarvest(UObject* WorldContextObject)
{
	PlayEngineSound(WorldContextObject, 0.5f, 1.8f);  // Bright pickup
}

void UTartariaSoundManager::PlayCombatHit(UObject* WorldContextObject)
{
	PlayEngineSound(WorldContextObject, 0.7f, 0.6f);  // Heavy thud
}

void UTartariaSoundManager::PlayNPCGreeting(UObject* WorldContextObject)
{
	PlayEngineSound(WorldContextObject, 0.4f, 1.2f);  // Warm chime
}

void UTartariaSoundManager::PlayMenuToggle(UObject* WorldContextObject)
{
	PlayEngineSound(WorldContextObject, 0.3f, 1.6f);  // Quick click
}

void UTartariaSoundManager::PlayForgeSeal(UObject* WorldContextObject)
{
	PlayEngineSound(WorldContextObject, 0.6f, 1.0f);  // Full, resonant
}

void UTartariaSoundManager::PlayFootstep(UObject* WorldContextObject, bool bSprinting)
{
	float Pitch = bSprinting ? 1.3f : 1.0f;
	float Volume = bSprinting ? 0.35f : 0.25f;
	PlayEngineSound(WorldContextObject, Volume, Pitch);
}

void UTartariaSoundManager::PlayDodge(UObject* WorldContextObject)
{
	PlayEngineSound(WorldContextObject, 0.4f, 2.5f);  // Quick whoosh
}

void UTartariaSoundManager::PlayLevelUp(UObject* WorldContextObject)
{
	PlayEngineSound(WorldContextObject, 0.8f, 1.3f);  // Fanfare
}

// =============================================================
// Biome Ambient Audio (Task #215)
// =============================================================

void UTartariaSoundManager::EnsureBiomeProfilesInitialized()
{
	if (bBiomeProfilesInitialized) return;
	bBiomeProfilesInitialized = true;

	// Pre-allocate for all 5 biome types
	const int32 NumBiomes = 5;
	BiomeProfiles.SetNum(NumBiomes);

	// CLEARINGHOUSE: Crowd murmur — low-mid frequency, soft, slow oscillation
	{
		FBiomeAudioProfile& P = BiomeProfiles[(int32)ETartariaBiome::Clearinghouse];
		P.BaseFrequencyHz = 200.f;
		P.RhythmHz = 0.3f;       // Slow crowd-like ebb and flow
		P.Volume = 0.25f;        // Background murmur, not intrusive
		P.ReverbDecay = 1.2f;    // Open marketplace, moderate reverb
		P.RhythmDepth = 0.15f;   // Subtle volume variation
	}

	// SCRIPTORIUM: Paper-rustle whisper — high frequency, very quiet, fast oscillation
	{
		FBiomeAudioProfile& P = BiomeProfiles[(int32)ETartariaBiome::Scriptorium];
		P.BaseFrequencyHz = 2000.f;
		P.RhythmHz = 4.0f;       // Fast fluttering = paper rustle feel
		P.Volume = 0.12f;        // Very quiet, intimate library
		P.ReverbDecay = 2.5f;    // Long reverb — cavernous archive hall
		P.RhythmDepth = 0.4f;    // More pronounced flutter
	}

	// MONOLITH_WARD: Bell-like tone — mid-low frequency, quiet, very slow cycle
	{
		FBiomeAudioProfile& P = BiomeProfiles[(int32)ETartariaBiome::MonolithWard];
		P.BaseFrequencyHz = 150.f;
		P.RhythmHz = 0.03f;      // ~33 second cycle — vast, meditative
		P.Volume = 0.2f;         // Mysterious, present but restrained
		P.ReverbDecay = 4.0f;    // Very long reverb — enormous stone chamber
		P.RhythmDepth = 0.35f;   // Noticeable swell and fade
	}

	// FORGE_DISTRICT: Industrial hammering — low frequency, medium volume, 1Hz rhythm
	{
		FBiomeAudioProfile& P = BiomeProfiles[(int32)ETartariaBiome::ForgeDistrict];
		P.BaseFrequencyHz = 80.f;
		P.RhythmHz = 1.0f;       // 1 Hz = hammering rhythm
		P.Volume = 0.4f;         // Louder, industrial presence
		P.ReverbDecay = 0.8f;    // Short reverb — dense, enclosed forge
		P.RhythmDepth = 0.5f;    // Strong pulsing for hammering feel
	}

	// VOID_REACH: Deep drone — very low frequency, medium volume, slow throb
	{
		FBiomeAudioProfile& P = BiomeProfiles[(int32)ETartariaBiome::VoidReach];
		P.BaseFrequencyHz = 40.f;
		P.RhythmHz = 0.1f;       // 10 second throb cycle — alien, unsettling
		P.Volume = 0.35f;        // Audible, oppressive presence
		P.ReverbDecay = 6.0f;    // Enormous reverb — infinite void
		P.RhythmDepth = 0.25f;   // Moderate pulse, constant menace
	}

	UE_LOG(LogTemp, Log, TEXT("TartariaSoundManager: Biome audio profiles initialized (%d biomes)"), NumBiomes);
}

const FBiomeAudioProfile& UTartariaSoundManager::GetBiomeProfile(ETartariaBiome Biome)
{
	EnsureBiomeProfilesInitialized();

	int32 Index = (int32)Biome;
	if (Index >= 0 && Index < BiomeProfiles.Num())
	{
		return BiomeProfiles[Index];
	}

	// Fallback to Clearinghouse
	return BiomeProfiles[(int32)ETartariaBiome::Clearinghouse];
}

ETartariaBiome UTartariaSoundManager::BiomeFromKey(const FString& BiomeKey)
{
	if (BiomeKey.Equals(TEXT("CLEARINGHOUSE"), ESearchCase::IgnoreCase))
		return ETartariaBiome::Clearinghouse;
	if (BiomeKey.Equals(TEXT("SCRIPTORIUM"), ESearchCase::IgnoreCase))
		return ETartariaBiome::Scriptorium;
	if (BiomeKey.Equals(TEXT("MONOLITH_WARD"), ESearchCase::IgnoreCase))
		return ETartariaBiome::MonolithWard;
	if (BiomeKey.Equals(TEXT("FORGE_DISTRICT"), ESearchCase::IgnoreCase))
		return ETartariaBiome::ForgeDistrict;
	if (BiomeKey.Equals(TEXT("VOID_REACH"), ESearchCase::IgnoreCase))
		return ETartariaBiome::VoidReach;

	UE_LOG(LogTemp, Warning,
		TEXT("TartariaSoundManager::BiomeFromKey: Unknown key '%s', defaulting to Clearinghouse"), *BiomeKey);
	return ETartariaBiome::Clearinghouse;
}

void UTartariaSoundManager::ConfigureBiomeAmbient(UAudioComponent* AudioComp, ETartariaBiome Biome)
{
	if (!AudioComp) return;

	EnsureBiomeProfilesInitialized();
	const FBiomeAudioProfile& Profile = GetBiomeProfile(Biome);

	// Load the engine's built-in looping sound as the tone source.
	// This sound is present in all UE5 installs and provides a tonal base
	// that we modulate with pitch to approximate the biome's frequency.
	static USoundBase* EngineBeep = LoadObject<USoundBase>(nullptr,
		TEXT("/Engine/VREditor/Sounds/UI/Object_PickUp"));

	if (EngineBeep)
	{
		AudioComp->SetSound(EngineBeep);
	}

	// Map BaseFrequencyHz to UE5 pitch multiplier.
	// Engine sound is ~440 Hz; scale proportionally.
	float PitchMult = FMath::Clamp(Profile.BaseFrequencyHz / 440.f, 0.1f, 4.5f);
	AudioComp->SetPitchMultiplier(PitchMult);

	// Set initial volume (will be modulated by TickBiomeAmbient)
	AudioComp->SetVolumeMultiplier(Profile.Volume);

	// Enable looping so the ambient tone repeats continuously
	AudioComp->bIsUISound = false;
	AudioComp->bAllowSpatialization = false;  // Ambient is non-directional

	UE_LOG(LogTemp, Log,
		TEXT("TartariaSoundManager: Configured biome ambient — biome=%d, freq=%.0fHz, pitch=%.2f, vol=%.2f, rhythm=%.2fHz, reverb=%.1fs"),
		(int32)Biome, Profile.BaseFrequencyHz, PitchMult, Profile.Volume,
		Profile.RhythmHz, Profile.ReverbDecay);
}

void UTartariaSoundManager::TickBiomeAmbient(UAudioComponent* AudioComp, ETartariaBiome Biome, float TimeAccumulator)
{
	if (!AudioComp || !AudioComp->IsPlaying()) return;

	EnsureBiomeProfilesInitialized();
	const FBiomeAudioProfile& Profile = GetBiomeProfile(Biome);

	// Apply rhythmic volume modulation (LFO-like effect).
	// Sine wave oscillates volume around the base level.
	// RhythmHz controls speed, RhythmDepth controls amplitude.
	float Phase = TimeAccumulator * Profile.RhythmHz * 2.f * PI;
	float Oscillation = FMath::Sin(Phase);  // Range: [-1, 1]

	// Map oscillation to volume: base +/- (depth * base)
	float ModulatedVolume = Profile.Volume * (1.f + Oscillation * Profile.RhythmDepth);
	ModulatedVolume = FMath::Clamp(ModulatedVolume, 0.01f, 1.0f);

	AudioComp->SetVolumeMultiplier(ModulatedVolume);

	// Pitch micro-variation: subtle drift for organic feel (+/- 2%)
	float PitchDrift = FMath::Sin(TimeAccumulator * 0.17f) * 0.02f;
	float BasePitch = FMath::Clamp(Profile.BaseFrequencyHz / 440.f, 0.1f, 4.5f);
	AudioComp->SetPitchMultiplier(BasePitch * (1.f + PitchDrift));
}

// =============================================================
// Material-Specific Impact Audio (Task #205)
// =============================================================

/**
 * Map a material's BasePitch (Hz) to a UE5 pitch multiplier.
 *
 * The engine sound source is a short blip near ~440 Hz.
 * We scale the pitch multiplier so that the perceived fundamental
 * approximates the profile's BasePitch value.
 *
 * Mapping: PitchMult = BasePitch / 440.0, clamped to [0.2, 4.0].
 */
static float MaterialPitchToEnginePitch(const FMaterialAudioProfile& Profile)
{
	float Raw = Profile.BasePitch / 440.f;
	return FMath::Clamp(Raw, 0.2f, 4.0f);
}

void UTartariaSoundManager::PlayMaterialImpact(UObject* WorldContextObject,
	ETartariaPhysicalMaterial Material, FVector Location, float Intensity)
{
	if (!WorldContextObject) return;

	EnsureProfilesInitialized();
	const FMaterialAudioProfile& Profile = GetMaterialProfile(Material);

	// Calculate pitch with randomized variance
	float BasePitchMult = MaterialPitchToEnginePitch(Profile);
	float PitchVariation = FMath::RandRange(-Profile.PitchVariance, Profile.PitchVariance);
	float FinalPitch = BasePitchMult * (1.f + PitchVariation);
	FinalPitch = FMath::Clamp(FinalPitch, 0.15f, 4.0f);

	// Volume scales with intensity
	float Volume = FMath::Clamp(Intensity * 0.5f, 0.1f, 1.0f);

	// Primary impact sound
	PlayEngineSoundAtLocation(WorldContextObject, Location, Volume, FinalPitch);

	// Resonance layer: for highly resonant materials (Metal, Crystal),
	// play a second, quieter, slightly higher-pitched sustain hit to
	// simulate the ringing tail.
	if (Profile.Resonance > 0.4f)
	{
		float ResonancePitch = FinalPitch * 1.5f;  // harmonic overtone
		float ResonanceVolume = Volume * Profile.Resonance * 0.4f;
		ResonancePitch = FMath::Clamp(ResonancePitch, 0.15f, 4.0f);
		ResonanceVolume = FMath::Clamp(ResonanceVolume, 0.05f, 0.5f);

		PlayEngineSoundAtLocation(WorldContextObject, Location, ResonanceVolume, ResonancePitch);
	}

#if WITH_EDITOR
	// Debug visualization: draw a colored sphere at impact point
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull))
	{
		DrawDebugSphere(World, Location, 15.f, 6,
			Profile.DebugColor.ToFColor(true), false, Profile.Decay);
	}
#endif

	UE_LOG(LogTemp, Verbose,
		TEXT("TartariaSoundManager: MaterialImpact mat=%d pitch=%.2f vol=%.2f res=%.2f loc=(%.0f,%.0f,%.0f)"),
		(int32)Material, FinalPitch, Volume, Profile.Resonance,
		Location.X, Location.Y, Location.Z);
}

void UTartariaSoundManager::PlayMaterialFootstep(UObject* WorldContextObject,
	ETartariaPhysicalMaterial Material, FVector Location, float SpeedFraction)
{
	if (!WorldContextObject) return;

	EnsureProfilesInitialized();
	const FMaterialAudioProfile& Profile = GetMaterialProfile(Material);

	// Footsteps are softer than full impacts.
	// Pitch is derived from material but shifted down slightly (feet don't
	// ring as hard as weapon strikes).
	float BasePitchMult = MaterialPitchToEnginePitch(Profile) * 0.8f;

	// Small random pitch variation to prevent mechanical repetition
	float PitchVariation = FMath::RandRange(-Profile.PitchVariance, Profile.PitchVariance);
	float FinalPitch = BasePitchMult * (1.f + PitchVariation);
	FinalPitch = FMath::Clamp(FinalPitch, 0.15f, 3.0f);

	// Volume: walking = soft, sprinting = louder. SpeedFraction maps 0-1.
	float BaseVolume = FMath::Lerp(0.12f, 0.35f, SpeedFraction);

	// Muffled materials (Earth) are even quieter; resonant ones (Metal) are sharper
	float MaterialVolumeScale = FMath::Lerp(0.7f, 1.2f, Profile.Resonance);
	float FinalVolume = FMath::Clamp(BaseVolume * MaterialVolumeScale, 0.05f, 0.5f);

	PlayEngineSoundAtLocation(WorldContextObject, Location, FinalVolume, FinalPitch);

	// For highly resonant surfaces (Metal walkways, Crystal floors),
	// add a faint ringing layer
	if (Profile.Resonance > 0.6f)
	{
		float RingPitch = FinalPitch * 2.0f;
		float RingVolume = FinalVolume * 0.2f * Profile.Resonance;
		RingPitch = FMath::Clamp(RingPitch, 0.2f, 4.0f);
		RingVolume = FMath::Clamp(RingVolume, 0.02f, 0.15f);

		PlayEngineSoundAtLocation(WorldContextObject, Location, RingVolume, RingPitch);
	}

#if WITH_EDITOR
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull))
	{
		DrawDebugPoint(World, Location, 8.f,
			Profile.DebugColor.ToFColor(true), false, 0.3f);
	}
#endif
}

void UTartariaSoundManager::PlayMaterialCombatHit(UObject* WorldContextObject,
	ETartariaPhysicalMaterial WeaponMaterial, ETartariaPhysicalMaterial TargetMaterial,
	FVector Location, float Damage)
{
	if (!WorldContextObject) return;

	EnsureProfilesInitialized();
	const FMaterialAudioProfile& WeaponProfile = GetMaterialProfile(WeaponMaterial);
	const FMaterialAudioProfile& TargetProfile = GetMaterialProfile(TargetMaterial);

	// Damage affects volume and pitch shift.
	// Higher damage => louder, slightly lower pitch (heavier impact feel).
	float DamageFraction = FMath::Clamp(Damage / 50.f, 0.2f, 1.5f);
	float DamagePitchShift = FMath::Lerp(1.0f, 0.75f, FMath::Clamp(DamageFraction, 0.f, 1.f));

	// ── Layer 1: Weapon ring ──────────────────────────────────────
	// The weapon material produces the "attack" sound component.
	// Metal sword = bright clang, Wood club = dull thwack.
	{
		float WeaponPitch = MaterialPitchToEnginePitch(WeaponProfile) * DamagePitchShift;
		float PitchVar = FMath::RandRange(-WeaponProfile.PitchVariance, WeaponProfile.PitchVariance);
		WeaponPitch *= (1.f + PitchVar);
		WeaponPitch = FMath::Clamp(WeaponPitch, 0.15f, 4.0f);

		float WeaponVolume = FMath::Clamp(0.4f * DamageFraction, 0.15f, 0.85f);
		// Resonant weapons (metal, crystal) ring louder
		WeaponVolume *= FMath::Lerp(0.8f, 1.3f, WeaponProfile.Resonance);
		WeaponVolume = FMath::Clamp(WeaponVolume, 0.1f, 1.0f);

		PlayEngineSoundAtLocation(WorldContextObject, Location, WeaponVolume, WeaponPitch);

		// Weapon resonance tail (e.g. metal sword ringing after hit)
		if (WeaponProfile.Resonance > 0.5f)
		{
			float TailPitch = WeaponPitch * 1.4f;
			float TailVolume = WeaponVolume * WeaponProfile.Resonance * 0.3f;
			TailPitch = FMath::Clamp(TailPitch, 0.2f, 4.0f);
			TailVolume = FMath::Clamp(TailVolume, 0.05f, 0.4f);

			PlayEngineSoundAtLocation(WorldContextObject, Location, TailVolume, TailPitch);
		}
	}

	// ── Layer 2: Target impact ────────────────────────────────────
	// The target material produces the "receiving" sound component.
	// Stone = crack, Earth = thump, Metal = clang.
	{
		float TargetPitch = MaterialPitchToEnginePitch(TargetProfile) * DamagePitchShift * 0.85f;
		float PitchVar = FMath::RandRange(-TargetProfile.PitchVariance, TargetProfile.PitchVariance);
		TargetPitch *= (1.f + PitchVar);
		TargetPitch = FMath::Clamp(TargetPitch, 0.15f, 3.5f);

		float TargetVolume = FMath::Clamp(0.35f * DamageFraction, 0.1f, 0.75f);
		// Deadened targets (Earth, Wood) absorb more, resonant targets (Metal) bounce
		TargetVolume *= FMath::Lerp(0.6f, 1.1f, TargetProfile.Resonance);
		TargetVolume = FMath::Clamp(TargetVolume, 0.1f, 0.9f);

		PlayEngineSoundAtLocation(WorldContextObject, Location, TargetVolume, TargetPitch);
	}

#if WITH_EDITOR
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull))
	{
		// Draw weapon color sphere + target color sphere, overlapping
		DrawDebugSphere(World, Location, 20.f, 8,
			WeaponProfile.DebugColor.ToFColor(true), false, 0.5f);
		DrawDebugSphere(World, Location, 12.f, 8,
			TargetProfile.DebugColor.ToFColor(true), false, 0.5f);
	}
#endif

	UE_LOG(LogTemp, Verbose,
		TEXT("TartariaSoundManager: CombatHit weapon=%d target=%d dmg=%.1f loc=(%.0f,%.0f,%.0f)"),
		(int32)WeaponMaterial, (int32)TargetMaterial, Damage,
		Location.X, Location.Y, Location.Z);
}
