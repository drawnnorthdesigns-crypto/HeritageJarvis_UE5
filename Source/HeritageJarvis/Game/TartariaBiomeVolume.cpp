#include "TartariaBiomeVolume.h"
#include "TartariaCharacter.h"
#include "Core/HJGameInstance.h"
#include "Core/HJApiClient.h"
#include "Components/SphereComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/AudioComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "GameFramework/Character.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/World.h"

ATartariaBiomeVolume::ATartariaBiomeVolume()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f; // 10 Hz tick for smooth lerping

	// Zone bounds — sphere collision for overlap detection
	ZoneBounds = CreateDefaultSubobject<USphereComponent>(TEXT("ZoneBounds"));
	ZoneBounds->SetSphereRadius(ZoneRadius);
	ZoneBounds->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	ZoneBounds->SetGenerateOverlapEvents(true);
	RootComponent = ZoneBounds;

	// Post-process volume for biome atmosphere (base biome look)
	BiomePostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("BiomePostProcess"));
	BiomePostProcess->SetupAttachment(RootComponent);
	BiomePostProcess->bEnabled = true;
	BiomePostProcess->bUnbound = false;

	// Post-process component for consequence-driven visual overlays
	ConsequencePostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("ConsequencePostProcess"));
	ConsequencePostProcess->SetupAttachment(RootComponent);
	ConsequencePostProcess->bEnabled = true;
	ConsequencePostProcess->bUnbound = false;
	ConsequencePostProcess->BlendWeight = 0.f; // Start invisible, ramp up when data arrives
	ConsequencePostProcess->Priority = 1.f;     // Layer on top of base biome PP

	// Exponential height fog for consequence-driven fog density
	ConsequenceFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("ConsequenceFog"));
	ConsequenceFog->SetupAttachment(RootComponent);
	ConsequenceFog->SetFogDensity(0.02f);
	ConsequenceFog->SetFogInscatteringColor(FLinearColor(0.5f, 0.5f, 0.6f, 1.f));
	ConsequenceFog->SetVisibility(false); // Hidden until first data arrives

	// Ambient audio placeholder
	AmbientAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("AmbientAudio"));
	AmbientAudio->SetupAttachment(RootComponent);
	AmbientAudio->bAutoActivate = false;
	AmbientAudio->VolumeMultiplier = 0.3f;

	// Ambient particle placeholder
	AmbientParticles = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("AmbientParticles"));
	AmbientParticles->SetupAttachment(RootComponent);
	AmbientParticles->bAutoActivate = false;
}

void ATartariaBiomeVolume::ConfigureBiomeAtmosphere()
{
	if (!BiomePostProcess) return;

	FPostProcessSettings& PP = BiomePostProcess->Settings;

	// ── CLEARINGHOUSE: Safe zone, warm amber, inviting ──────────────
	if (BiomeKey == TEXT("CLEARINGHOUSE"))
	{
		PP.bOverride_BloomIntensity = true;
		PP.BloomIntensity = 0.5f;

		PP.bOverride_VignetteIntensity = true;
		PP.VignetteIntensity = 0.3f;

		PP.bOverride_AutoExposureBias = true;
		PP.AutoExposureBias = 0.0f;

		// Warm color grading — slight amber push
		PP.bOverride_ColorGamma = true;
		PP.ColorGamma = FVector4(1.0f, 0.98f, 0.92f, 1.0f);

		PP.bOverride_ColorSaturation = true;
		PP.ColorSaturation = FVector4(1.05f, 1.05f, 1.05f, 1.0f);

		PP.bOverride_WhiteTemp = true;
		PP.WhiteTemp = 6200.f; // Slightly warm neutral
	}
	// ── SCRIPTORIUM: Dusty blue, contemplative, desaturated ─────────
	else if (BiomeKey == TEXT("SCRIPTORIUM"))
	{
		PP.bOverride_BloomIntensity = true;
		PP.BloomIntensity = 0.3f;

		PP.bOverride_VignetteIntensity = true;
		PP.VignetteIntensity = 0.45f;

		PP.bOverride_AutoExposureBias = true;
		PP.AutoExposureBias = -0.3f; // Slightly darker, library feel

		PP.bOverride_ColorGamma = true;
		PP.ColorGamma = FVector4(0.92f, 0.95f, 1.05f, 1.0f); // Cool blue shift

		PP.bOverride_ColorSaturation = true;
		PP.ColorSaturation = FVector4(0.85f, 0.85f, 0.85f, 1.0f); // Desaturated

		PP.bOverride_WhiteTemp = true;
		PP.WhiteTemp = 7500.f; // Cool temperature
	}
	// ── MONOLITH_WARD: Deep purple, mysterious, crushed blacks ──────
	else if (BiomeKey == TEXT("MONOLITH_WARD"))
	{
		PP.bOverride_BloomIntensity = true;
		PP.BloomIntensity = 0.8f; // Strong bloom for monolith glow

		PP.bOverride_VignetteIntensity = true;
		PP.VignetteIntensity = 0.6f; // Heavy vignette, claustrophobic

		PP.bOverride_AutoExposureBias = true;
		PP.AutoExposureBias = -0.5f; // Dark, foreboding

		PP.bOverride_ColorGamma = true;
		PP.ColorGamma = FVector4(0.88f, 0.85f, 1.0f, 1.0f); // Purple push

		PP.bOverride_ColorSaturation = true;
		PP.ColorSaturation = FVector4(1.1f, 0.9f, 1.2f, 1.0f); // Boost purple, reduce green

		PP.bOverride_ColorContrast = true;
		PP.ColorContrast = FVector4(1.15f, 1.15f, 1.15f, 1.0f); // Crushed blacks

		PP.bOverride_WhiteTemp = true;
		PP.WhiteTemp = 5000.f; // Neutral-warm
	}
	// ── FORGE_DISTRICT: Smoky orange, industrial, high contrast ─────
	else if (BiomeKey == TEXT("FORGE_DISTRICT"))
	{
		PP.bOverride_BloomIntensity = true;
		PP.BloomIntensity = 0.6f;

		PP.bOverride_VignetteIntensity = true;
		PP.VignetteIntensity = 0.35f;

		PP.bOverride_AutoExposureBias = true;
		PP.AutoExposureBias = 0.2f; // Slightly brighter, fire-lit

		PP.bOverride_ColorGamma = true;
		PP.ColorGamma = FVector4(1.08f, 0.95f, 0.82f, 1.0f); // Strong warm orange push

		PP.bOverride_ColorSaturation = true;
		PP.ColorSaturation = FVector4(1.15f, 1.0f, 0.8f, 1.0f); // Saturate reds, desaturate blues

		PP.bOverride_ColorContrast = true;
		PP.ColorContrast = FVector4(1.2f, 1.2f, 1.2f, 1.0f); // High contrast, industrial

		PP.bOverride_WhiteTemp = true;
		PP.WhiteTemp = 4500.f; // Very warm, forge light
	}
	// ── VOID_REACH: Black-green, alien, extreme desaturation ────────
	else if (BiomeKey == TEXT("VOID_REACH"))
	{
		PP.bOverride_BloomIntensity = true;
		PP.BloomIntensity = 0.2f; // Minimal bloom, harsh void

		PP.bOverride_VignetteIntensity = true;
		PP.VignetteIntensity = 0.7f; // Very heavy vignette, tunnel vision

		PP.bOverride_AutoExposureBias = true;
		PP.AutoExposureBias = -0.8f; // Very dark, void-like

		PP.bOverride_ColorGamma = true;
		PP.ColorGamma = FVector4(0.85f, 0.92f, 0.85f, 1.0f); // Slight sickly green

		PP.bOverride_ColorSaturation = true;
		PP.ColorSaturation = FVector4(0.6f, 0.6f, 0.6f, 1.0f); // Heavily desaturated

		PP.bOverride_ColorContrast = true;
		PP.ColorContrast = FVector4(1.3f, 1.3f, 1.3f, 1.0f); // Extreme contrast

		PP.bOverride_WhiteTemp = true;
		PP.WhiteTemp = 9000.f; // Very cold, alien
	}

	// ── Apply enhanced per-biome post-process (Task #208) ───────────
	ConfigureBiomePostProcess();

	// Apply blend weight so transitions feel smooth (volumetric blend)
	BiomePostProcess->BlendWeight = 1.0f;
	BiomePostProcess->BlendRadius = ZoneRadius * 0.3f; // 30% of zone = transition zone

	UE_LOG(LogTemp, Log, TEXT("TartariaBiomeVolume: Configured atmosphere for %s"), *BiomeKey);
}

// =====================================================================
// Enhanced Per-Biome Post-Process (Task #208)
// =====================================================================

void ATartariaBiomeVolume::ConfigureBiomePostProcess()
{
	if (!BiomePostProcess) return;

	FPostProcessSettings& PP = BiomePostProcess->Settings;

	// ── CLEARINGHOUSE: Warm neutral tint, clean visibility, golden hour ──
	if (BiomeKey == TEXT("CLEARINGHOUSE"))
	{
		// SceneColorTint — subtle warm shift (golden hour feel)
		PP.bOverride_SceneColorTint = true;
		PP.SceneColorTint = FLinearColor(1.0f, 0.98f, 0.95f, 1.0f);

		// Bloom — clean, slight glow
		PP.bOverride_BloomIntensity = true;
		PP.BloomIntensity = 0.8f;
		PP.bOverride_BloomThreshold = true;
		PP.BloomThreshold = 1.0f;

		// ColorGain — warm golden hour shift
		PP.bOverride_ColorGain = true;
		PP.ColorGain = FVector4(1.0f, 0.95f, 0.9f, 1.0f);

		// No film grain — clean hub area
		PP.bOverride_FilmGrainIntensity = true;
		PP.FilmGrainIntensity = 0.0f;

		// No chromatic aberration — safe zone clarity
		PP.bOverride_SceneFringeIntensity = true;
		PP.SceneFringeIntensity = 0.0f;
	}
	// ── SCRIPTORIUM: Warm amber tint, dusty library mood ────────────
	else if (BiomeKey == TEXT("SCRIPTORIUM"))
	{
		// SceneColorTint — warm amber for parchment/library feel
		PP.bOverride_SceneColorTint = true;
		PP.SceneColorTint = FLinearColor(1.0f, 0.85f, 0.6f, 1.0f);

		// Slight vignette for intimate "reading" feel
		PP.bOverride_VignetteIntensity = true;
		PP.VignetteIntensity = 0.3f;

		// Lower bloom — subdued candlelight glow
		PP.bOverride_BloomIntensity = true;
		PP.BloomIntensity = 0.4f;
		PP.bOverride_BloomThreshold = true;
		PP.BloomThreshold = 0.8f;

		// Lower contrast for dusty, muted library mood
		PP.bOverride_ColorContrast = true;
		PP.ColorContrast = FVector4(0.95f, 0.95f, 0.95f, 1.0f);

		// No chromatic aberration — calm scholarly environment
		PP.bOverride_SceneFringeIntensity = true;
		PP.SceneFringeIntensity = 0.0f;

		// No film grain
		PP.bOverride_FilmGrainIntensity = true;
		PP.FilmGrainIntensity = 0.0f;
	}
	// ── MONOLITH_WARD: Cool blue-grey, dramatic ominous shadows ─────
	else if (BiomeKey == TEXT("MONOLITH_WARD"))
	{
		// SceneColorTint — cool blue-grey for ancient stone
		PP.bOverride_SceneColorTint = true;
		PP.SceneColorTint = FLinearColor(0.8f, 0.85f, 1.0f, 1.0f);

		// High contrast for dramatic shadow play
		PP.bOverride_ColorContrast = true;
		PP.ColorContrast = FVector4(1.1f, 1.1f, 1.1f, 1.0f);

		// Stronger vignette for ominous, oppressive feel
		PP.bOverride_VignetteIntensity = true;
		PP.VignetteIntensity = 0.5f;

		// Film grain for ancient texture
		PP.bOverride_FilmGrainIntensity = true;
		PP.FilmGrainIntensity = 0.1f;

		// No chromatic aberration — stark clarity
		PP.bOverride_SceneFringeIntensity = true;
		PP.SceneFringeIntensity = 0.0f;

		// Bloom at baseline
		PP.bOverride_BloomIntensity = true;
		PP.BloomIntensity = 0.6f;
	}
	// ── FORGE_DISTRICT: Orange heat haze, industrial distortion ──────
	else if (BiomeKey == TEXT("FORGE_DISTRICT"))
	{
		// SceneColorTint — orange heat haze
		PP.bOverride_SceneColorTint = true;
		PP.SceneColorTint = FLinearColor(1.1f, 0.8f, 0.5f, 1.0f);

		// Higher bloom for heat shimmer
		PP.bOverride_BloomIntensity = true;
		PP.BloomIntensity = 1.2f;
		PP.bOverride_BloomThreshold = true;
		PP.BloomThreshold = 0.6f;

		// Warm color grading — shadows shifted orange
		PP.bOverride_ColorGain = true;
		PP.ColorGain = FVector4(1.1f, 0.85f, 0.6f, 1.0f);

		// Chromatic aberration for heat distortion
		PP.bOverride_SceneFringeIntensity = true;
		PP.SceneFringeIntensity = 1.5f;

		// No film grain — heat shimmer via bloom is enough
		PP.bOverride_FilmGrainIntensity = true;
		PP.FilmGrainIntensity = 0.0f;
	}
	// ── VOID_REACH: Purple desaturated, reality-bending ─────────────
	else if (BiomeKey == TEXT("VOID_REACH"))
	{
		// SceneColorTint — purple desaturated
		PP.bOverride_SceneColorTint = true;
		PP.SceneColorTint = FLinearColor(0.7f, 0.5f, 0.9f, 1.0f);

		// Heavy desaturation — color drained from reality
		PP.bOverride_ColorSaturation = true;
		PP.ColorSaturation = FVector4(0.5f, 0.5f, 0.5f, 1.0f);

		// Strong vignette — reality collapsing at edges
		PP.bOverride_VignetteIntensity = true;
		PP.VignetteIntensity = 0.6f;

		// Chromatic aberration — reality bending
		PP.bOverride_SceneFringeIntensity = true;
		PP.SceneFringeIntensity = 3.0f;

		// Film grain — unstable reality texture
		PP.bOverride_FilmGrainIntensity = true;
		PP.FilmGrainIntensity = 0.15f;

		// Minimal bloom — void swallows light
		PP.bOverride_BloomIntensity = true;
		PP.BloomIntensity = 0.3f;
	}

	UE_LOG(LogTemp, Log, TEXT("TartariaBiomeVolume: Enhanced post-process configured for %s"), *BiomeKey);
}

void ATartariaBiomeVolume::BeginPlay()
{
	Super::BeginPlay();

	// Update sphere radius from property (may have been edited in editor)
	ZoneBounds->SetSphereRadius(ZoneRadius);

	// Bind overlap events
	ZoneBounds->OnComponentBeginOverlap.AddDynamic(this, &ATartariaBiomeVolume::OnZoneBeginOverlap);
	ZoneBounds->OnComponentEndOverlap.AddDynamic(this, &ATartariaBiomeVolume::OnZoneEndOverlap);

	// Set consequence post-process blend radius to match biome PP
	if (ConsequencePostProcess)
	{
		ConsequencePostProcess->BlendRadius = ZoneRadius * 0.3f;
	}

	// Kick off the first consequence fetch immediately
	ConsequencePollTimer = ConsequencePollIntervalSec;

	UE_LOG(LogTemp, Log, TEXT("TartariaBiomeVolume: %s initialized (radius=%.0f, difficulty=%d)"),
		*BiomeKey, ZoneRadius, Difficulty);
}

void ATartariaBiomeVolume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ── Consequence polling ──────────────────────────────────────────
	ConsequencePollTimer += DeltaTime;
	if (ConsequencePollTimer >= ConsequencePollIntervalSec)
	{
		ConsequencePollTimer = 0.f;
		FetchConsequenceModifiers();
	}

	// ── Apply consequence visuals (smooth interpolation) ─────────────
	ApplyConsequenceModifiers(DeltaTime);
}

// =====================================================================
// World Consequence Visual Modifiers (Task #202)
// =====================================================================

void ATartariaBiomeVolume::FetchConsequenceModifiers()
{
	// Guard against overlapping requests
	if (bConsequenceFetchInFlight) return;

	UWorld* World = GetWorld();
	if (!World) return;

	UHJGameInstance* GI = UHJGameInstance::Get(World);
	if (!GI || !GI->ApiClient) return;

	// Build endpoint: /api/game/world-consequences/FORGE_DISTRICT
	FString Endpoint = FString::Printf(TEXT("/api/game/world-consequences/%s"), *BiomeKey);

	bConsequenceFetchInFlight = true;

	FOnApiResponse CB;
	CB.BindUObject(this, &ATartariaBiomeVolume::OnConsequenceResponse);
	GI->ApiClient->Get(Endpoint, CB);
}

void ATartariaBiomeVolume::OnConsequenceResponse(bool bSuccess, const FString& Body)
{
	bConsequenceFetchInFlight = false;

	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("TartariaBiomeVolume: Consequence fetch failed for %s"), *BiomeKey);
		return;
	}

	// Parse JSON response:
	// {
	//   "zone": "FORGE_DISTRICT",
	//   "modifiers": {
	//     "tint": [0.9, 0.7, 0.5],
	//     "fog_density": 0.03,
	//     "saturation": 0.8,
	//     "description": "The forges burn bright"
	//   },
	//   "health": 75,
	//   "flags": ["forge_active", "prosperity"]
	// }

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("TartariaBiomeVolume: Failed to parse consequence JSON for %s"), *BiomeKey);
		return;
	}

	// Parse health
	double HealthVal = 50.0;
	Root->TryGetNumberField(TEXT("health"), HealthVal);
	ZoneHealth = FMath::Clamp(static_cast<int32>(HealthVal), 0, 100);

	// Parse flags to detect corruption
	bCorruptionActive = false;
	const TArray<TSharedPtr<FJsonValue>>* FlagsArray = nullptr;
	if (Root->TryGetArrayField(TEXT("flags"), FlagsArray) && FlagsArray)
	{
		for (const TSharedPtr<FJsonValue>& FlagVal : *FlagsArray)
		{
			FString FlagStr;
			if (FlagVal->TryGetString(FlagStr))
			{
				if (FlagStr.Equals(TEXT("CORRUPTED"), ESearchCase::IgnoreCase))
				{
					bCorruptionActive = true;
				}
			}
		}
	}

	// Parse modifiers object
	const TSharedPtr<FJsonObject>* ModifiersObj = nullptr;
	if (Root->TryGetObjectField(TEXT("modifiers"), ModifiersObj) && ModifiersObj)
	{
		// Parse tint array [r, g, b]
		const TArray<TSharedPtr<FJsonValue>>* TintArray = nullptr;
		if ((*ModifiersObj)->TryGetArrayField(TEXT("tint"), TintArray) && TintArray && TintArray->Num() >= 3)
		{
			double R = 1.0, G = 1.0, B = 1.0;
			(*TintArray)[0]->TryGetNumber(R);
			(*TintArray)[1]->TryGetNumber(G);
			(*TintArray)[2]->TryGetNumber(B);
			TargetTint = FLinearColor(
				static_cast<float>(R),
				static_cast<float>(G),
				static_cast<float>(B),
				1.f
			);
		}

		// Parse fog_density
		double FogDensity = 0.02;
		if ((*ModifiersObj)->TryGetNumberField(TEXT("fog_density"), FogDensity))
		{
			TargetFogDensity = static_cast<float>(FogDensity);
		}

		// Parse saturation
		double Saturation = 1.0;
		if ((*ModifiersObj)->TryGetNumberField(TEXT("saturation"), Saturation))
		{
			TargetSaturation = static_cast<float>(Saturation);
		}

		// Parse description
		FString Description;
		if ((*ModifiersObj)->TryGetStringField(TEXT("description"), Description))
		{
			LastDescription = Description;
		}
	}

	// Enable consequence fog once we receive valid data
	if (ConsequenceFog && !ConsequenceFog->IsVisible())
	{
		ConsequenceFog->SetVisibility(true);
	}

	// Ramp consequence PP blend weight to full
	if (ConsequencePostProcess && ConsequencePostProcess->BlendWeight < 1.f)
	{
		ConsequencePostProcess->BlendWeight = 1.f;
	}

	// Broadcast delegate for UI/gameplay hooks
	OnConsequenceModifiersUpdated.Broadcast(ZoneHealth, bCorruptionActive, LastDescription);

	UE_LOG(LogTemp, Log,
		TEXT("TartariaBiomeVolume: %s consequence update — health=%d, corruption=%s, tint=(%.2f,%.2f,%.2f), fog=%.3f, sat=%.2f"),
		*BiomeKey, ZoneHealth,
		bCorruptionActive ? TEXT("YES") : TEXT("NO"),
		TargetTint.R, TargetTint.G, TargetTint.B,
		TargetFogDensity, TargetSaturation);
}

void ATartariaBiomeVolume::ApplyConsequenceModifiers(float DeltaTime)
{
	// ── Lerp current values toward targets ───────────────────────────
	const float Alpha = FMath::Clamp(ConsequenceLerpSpeed * DeltaTime, 0.f, 1.f);

	CurrentTint = FMath::Lerp(CurrentTint, TargetTint, Alpha);
	CurrentFogDensity = FMath::Lerp(CurrentFogDensity, TargetFogDensity, Alpha);
	CurrentSaturation = FMath::Lerp(CurrentSaturation, TargetSaturation, Alpha);

	// ── Corruption pulse (sinusoidal fog oscillation) ────────────────
	float FogToApply = CurrentFogDensity;
	FLinearColor TintToApply = CurrentTint;

	if (bCorruptionActive)
	{
		CorruptionPulseAccumulator += DeltaTime;

		// Pulsing fog: base density +/- 30% at 0.5 Hz
		float PulseWave = FMath::Sin(CorruptionPulseAccumulator * PI * 0.5f);
		FogToApply = CurrentFogDensity * (1.f + 0.3f * PulseWave);

		// Subtle purple pulse on tint
		float TintPulse = 0.05f * PulseWave;
		TintToApply.R = FMath::Clamp(CurrentTint.R - TintPulse, 0.f, 2.f);
		TintToApply.G = FMath::Clamp(CurrentTint.G - TintPulse * 2.f, 0.f, 2.f);
		TintToApply.B = FMath::Clamp(CurrentTint.B + TintPulse, 0.f, 2.f);
	}
	else
	{
		CorruptionPulseAccumulator = 0.f;
	}

	// ── Apply to consequence post-process ────────────────────────────
	if (ConsequencePostProcess)
	{
		FPostProcessSettings& PP = ConsequencePostProcess->Settings;

		// Color grading via SceneColorTint (multiplicative color filter)
		PP.bOverride_SceneColorTint = true;
		PP.SceneColorTint = TintToApply;

		// Saturation — applied uniformly to all channels
		PP.bOverride_ColorSaturation = true;
		PP.ColorSaturation = FVector4(
			CurrentSaturation,
			CurrentSaturation,
			CurrentSaturation,
			1.0f
		);

		// Additional vignette for desolation (low health)
		if (ZoneHealth < 25)
		{
			PP.bOverride_VignetteIntensity = true;
			PP.VignetteIntensity = FMath::Lerp(0.f, 0.4f, 1.f - (ZoneHealth / 25.f));
		}
		else
		{
			PP.bOverride_VignetteIntensity = true;
			PP.VignetteIntensity = 0.f;
		}

		// Chromatic aberration for corruption
		PP.bOverride_SceneFringeIntensity = true;
		PP.SceneFringeIntensity = bCorruptionActive ? 3.f : 0.f;
	}

	// ── Apply to consequence fog component ───────────────────────────
	if (ConsequenceFog && ConsequenceFog->IsVisible())
	{
		ConsequenceFog->SetFogDensity(FMath::Clamp(FogToApply, 0.001f, 0.5f));

		// Tint fog inscattering color to match consequence tint
		FLinearColor FogColor(
			TintToApply.R * 0.5f,
			TintToApply.G * 0.5f,
			TintToApply.B * 0.5f,
			1.f
		);
		ConsequenceFog->SetFogInscatteringColor(FogColor);
	}
}

// =====================================================================
// Overlap Events
// =====================================================================

void ATartariaBiomeVolume::OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent,
                                                AActor* OtherActor, UPrimitiveComponent* OtherComp,
                                                int32 OtherBodyIndex, bool bFromSweep,
                                                const FHitResult& SweepResult)
{
	ATartariaCharacter* PlayerChar = Cast<ATartariaCharacter>(OtherActor);
	if (!PlayerChar) return;

	UE_LOG(LogTemp, Log, TEXT("TartariaBiomeVolume: Player entered %s biome (difficulty %d)"),
		*BiomeKey, Difficulty);

	// Update player's current biome
	PlayerChar->CurrentBiome = BiomeKey;

	// Track player presence for consequence visuals
	bPlayerInside = true;

	// Always broadcast zone change
	OnZoneEntered.Broadcast(BiomeKey, Difficulty);

	// Activate ambient atmosphere
	if (AmbientAudio && !AmbientAudio->IsPlaying())
	{
		AmbientAudio->Play();
	}
	if (AmbientParticles && !AmbientParticles->IsActive())
	{
		AmbientParticles->Activate();
	}

	// Immediate consequence fetch on zone entry for responsive visuals
	FetchConsequenceModifiers();

	// Threat check with cooldown
	UWorld* World = GetWorld();
	if (!World) return;

	float Now = World->GetTimeSeconds();
	if (Now - LastThreatCheckTime < ThreatCheckCooldownSec) return;
	LastThreatCheckTime = Now;

	// Skip threat check for safe zones (difficulty 1)
	if (Difficulty <= 1) return;

	// Call /api/game/threat/check
	UHJGameInstance* GI = UHJGameInstance::Get(World);
	if (!GI || !GI->ApiClient) return;

	// Build JSON body: {location: biome_key, player_defense: defense}
	TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject());
	Body->SetStringField(TEXT("location"), BiomeKey.ToLower());
	Body->SetNumberField(TEXT("player_defense"), PlayerChar->Defense);

	FString BodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	FOnApiResponse CB;
	CB.BindUObject(this, &ATartariaBiomeVolume::OnThreatCheckResponse);
	GI->ApiClient->Post(TEXT("/api/game/threat/check"), BodyStr, CB);
}

void ATartariaBiomeVolume::OnZoneEndOverlap(UPrimitiveComponent* OverlappedComponent,
                                              AActor* OtherActor, UPrimitiveComponent* OtherComp,
                                              int32 OtherBodyIndex)
{
	if (ACharacter* PlayerChar = Cast<ACharacter>(OtherActor))
	{
		UE_LOG(LogTemp, Log, TEXT("TartariaBiomeVolume: Player left %s biome"), *BiomeKey);

		bPlayerInside = false;

		// Deactivate ambient atmosphere
		if (AmbientAudio && AmbientAudio->IsPlaying())
		{
			AmbientAudio->FadeOut(2.0f, 0.0f);
		}
		if (AmbientParticles && AmbientParticles->IsActive())
		{
			AmbientParticles->Deactivate();
		}
	}
}

// =====================================================================
// Threat Check Response
// =====================================================================

void ATartariaBiomeVolume::OnThreatCheckResponse(bool bSuccess, const FString& Body)
{
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("TartariaBiomeVolume: Threat check failed for %s"), *BiomeKey);
		return;
	}

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

	bool bSafe = true;
	Root->TryGetBoolField(TEXT("safe"), bSafe);

	if (bSafe)
	{
		UE_LOG(LogTemp, Log, TEXT("TartariaBiomeVolume: %s zone is safe"), *BiomeKey);
		return;
	}

	// Parse threat info
	FTartariaThreatInfo Threat;
	Threat.bSafe = false;
	Threat.BiomeKey = BiomeKey;
	Root->TryGetStringField(TEXT("enemy"), Threat.EnemyKey);
	Root->TryGetStringField(TEXT("message"), Threat.Message);

	double Power = 0, EffPower = 0;
	Root->TryGetNumberField(TEXT("enemy_power"), Power);
	Root->TryGetNumberField(TEXT("effective_power"), EffPower);
	Threat.EnemyPower = static_cast<int32>(Power);
	Threat.EffectivePower = static_cast<int32>(EffPower);

	UE_LOG(LogTemp, Warning, TEXT("TartariaBiomeVolume: THREAT in %s — %s (power %d)"),
		*BiomeKey, *Threat.EnemyKey, Threat.EnemyPower);

	OnThreatDetected.Broadcast(Threat, this);
}
