#include "TartariaBiomeVolume.h"
#include "TartariaCharacter.h"
#include "Core/HJGameInstance.h"
#include "Core/HJApiClient.h"
#include "Components/SphereComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/AudioComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "GameFramework/Character.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/World.h"

ATartariaBiomeVolume::ATartariaBiomeVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	// Zone bounds — sphere collision for overlap detection
	ZoneBounds = CreateDefaultSubobject<USphereComponent>(TEXT("ZoneBounds"));
	ZoneBounds->SetSphereRadius(ZoneRadius);
	ZoneBounds->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	ZoneBounds->SetGenerateOverlapEvents(true);
	RootComponent = ZoneBounds;

	// Post-process volume for biome atmosphere
	BiomePostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("BiomePostProcess"));
	BiomePostProcess->SetupAttachment(RootComponent);
	BiomePostProcess->bEnabled = true;
	BiomePostProcess->bUnbound = false;

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

	// Apply blend weight so transitions feel smooth (volumetric blend)
	BiomePostProcess->BlendWeight = 1.0f;
	BiomePostProcess->BlendRadius = ZoneRadius * 0.3f; // 30% of zone = transition zone

	UE_LOG(LogTemp, Log, TEXT("TartariaBiomeVolume: Configured atmosphere for %s"), *BiomeKey);
}

void ATartariaBiomeVolume::BeginPlay()
{
	Super::BeginPlay();

	// Update sphere radius from property (may have been edited in editor)
	ZoneBounds->SetSphereRadius(ZoneRadius);

	// Bind overlap events
	ZoneBounds->OnComponentBeginOverlap.AddDynamic(this, &ATartariaBiomeVolume::OnZoneBeginOverlap);
	ZoneBounds->OnComponentEndOverlap.AddDynamic(this, &ATartariaBiomeVolume::OnZoneEndOverlap);

	UE_LOG(LogTemp, Log, TEXT("TartariaBiomeVolume: %s initialized (radius=%.0f, difficulty=%d)"),
		*BiomeKey, ZoneRadius, Difficulty);
}

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
