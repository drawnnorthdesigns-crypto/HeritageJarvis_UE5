#include "TartariaAmbientSound.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundAttenuation.h"
#include "Engine/World.h"

ATartariaAmbientSound::ATartariaAmbientSound()
{
	PrimaryActorTick.bCanEverTick = false;

	// Root component
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// Audio component
	AudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("AmbientAudio"));
	AudioComp->SetupAttachment(RootComponent);
	AudioComp->bAutoActivate = false;
	AudioComp->bIsUISound = false;
	AudioComp->bAllowSpatialization = true;
	AudioComp->bOverrideAttenuation = true;

	// Default attenuation: linear falloff over radius
	AudioComp->AttenuationOverrides.bAttenuate = true;
	AudioComp->AttenuationOverrides.FalloffDistance = AttenuationRadius;
	AudioComp->AttenuationOverrides.bSpatialize = true;
}

void ATartariaAmbientSound::BeginPlay()
{
	Super::BeginPlay();
	StartAmbient();
}

void ATartariaAmbientSound::ConfigureForBiome()
{
	// Per-biome audio configuration: volume, pitch, attenuation radius
	// These create distinct acoustic identities even before sound assets are assigned

	if (BiomeKey == TEXT("CLEARINGHOUSE"))
	{
		// Safe zone: warm, moderate volume, wide radius
		Volume = 0.4f;
		PitchMultiplier = 1.0f;
		AttenuationRadius = 60000.f;
		FadeInDuration = 4.0f;
	}
	else if (BiomeKey == TEXT("SCRIPTORIUM"))
	{
		// Library: quiet, slightly lower pitch (deep hum), tight radius
		Volume = 0.25f;
		PitchMultiplier = 0.85f;
		AttenuationRadius = 40000.f;
		FadeInDuration = 5.0f;
	}
	else if (BiomeKey == TEXT("MONOLITH_WARD"))
	{
		// Mysterious: moderate volume, slow pulse via low pitch
		Volume = 0.35f;
		PitchMultiplier = 0.7f;
		AttenuationRadius = 50000.f;
		FadeInDuration = 3.0f;
	}
	else if (BiomeKey == TEXT("FORGE_DISTRICT"))
	{
		// Industrial: louder, normal pitch, wide spread
		Volume = 0.55f;
		PitchMultiplier = 1.1f;
		AttenuationRadius = 55000.f;
		FadeInDuration = 2.0f;
	}
	else if (BiomeKey == TEXT("VOID_REACH"))
	{
		// Alien: quiet but unsettling, high pitch, tight radius
		Volume = 0.3f;
		PitchMultiplier = 1.3f;
		AttenuationRadius = 45000.f;
		FadeInDuration = 6.0f;
	}

	// Apply updated attenuation
	AudioComp->AttenuationOverrides.FalloffDistance = AttenuationRadius;

	UE_LOG(LogTemp, Log, TEXT("TartariaAmbientSound: Configured for %s (vol=%.2f, pitch=%.2f, radius=%.0f)"),
		*BiomeKey, Volume, PitchMultiplier, AttenuationRadius);
}

void ATartariaAmbientSound::StartAmbient()
{
	if (!AudioComp) return;

	// Assign sound cue if set
	if (BiomeSoundCue)
	{
		AudioComp->SetSound(BiomeSoundCue);
	}
	else
	{
		// No sound asset assigned — stay silent (common for first build, assets added later)
		UE_LOG(LogTemp, Log, TEXT("TartariaAmbientSound: No sound cue for %s — silent. Assign BiomeSoundCue in editor."),
			*BiomeKey);
		return;
	}

	AudioComp->SetVolumeMultiplier(Volume);
	AudioComp->SetPitchMultiplier(PitchMultiplier);
	AudioComp->FadeIn(FadeInDuration, Volume);

	UE_LOG(LogTemp, Log, TEXT("TartariaAmbientSound: Playing ambient for %s"), *BiomeKey);
}

void ATartariaAmbientSound::StopAmbient(float FadeOutDuration)
{
	if (!AudioComp) return;
	AudioComp->FadeOut(FadeOutDuration, 0.f);
}
