#include "TartariaBiomeParticles.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"

ATartariaBiomeParticles::ATartariaBiomeParticles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f; // Only tick twice per second for player tracking

	// Scene root
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// Niagara component — created here, activated on demand
	NiagaraComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraParticles"));
	NiagaraComp->SetupAttachment(RootComponent);
	NiagaraComp->bAutoActivate = false;

	// CRITICAL: Use LOCAL SPACE to survive origin rebasing during interplanetary transit
	NiagaraComp->SetAbsolute(false, false, false);
}

void ATartariaBiomeParticles::BeginPlay()
{
	Super::BeginPlay();

	// Start particles if configured
	if (bParticlesActive || NiagaraSystemAsset)
	{
		ActivateParticles();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("TartariaBiomeParticles: %s — no NiagaraSystem assigned. "
			"Assign NiagaraSystemAsset in editor for visual particles."), *BiomeKey);
	}
}

void ATartariaBiomeParticles::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	FollowPlayer();
}

void ATartariaBiomeParticles::ConfigureForBiome()
{
	// Per-biome particle profiles
	// These parameters feed into the Niagara system via user variables

	if (BiomeKey == TEXT("CLEARINGHOUSE"))
	{
		// Warm dust motes: golden, gentle drift, wide spread
		ParticleColor = FLinearColor(0.85f, 0.72f, 0.45f, 0.35f);
		SpawnRate = 12.f;
		SpawnRadius = 6000.f;
		ParticleScale = 0.8f;
		DriftSpeed = 30.f;
		ParticleLifetime = 10.f;
	}
	else if (BiomeKey == TEXT("SCRIPTORIUM"))
	{
		// Floating luminous motes: cool blue-white, slow, sparse
		ParticleColor = FLinearColor(0.6f, 0.7f, 0.95f, 0.25f);
		SpawnRate = 6.f;
		SpawnRadius = 4000.f;
		ParticleScale = 1.2f;
		DriftSpeed = 15.f;
		ParticleLifetime = 14.f;
	}
	else if (BiomeKey == TEXT("MONOLITH_WARD"))
	{
		// Dark wisps: purple-gray, moderate rate, slow swirl
		ParticleColor = FLinearColor(0.45f, 0.35f, 0.6f, 0.4f);
		SpawnRate = 10.f;
		SpawnRadius = 5000.f;
		ParticleScale = 1.5f;
		DriftSpeed = -20.f; // Negative = downward drift (sinking wisps)
		ParticleLifetime = 12.f;
	}
	else if (BiomeKey == TEXT("FORGE_DISTRICT"))
	{
		// Ember sparks: orange-red, fast rise, high spawn rate
		ParticleColor = FLinearColor(1.0f, 0.45f, 0.1f, 0.6f);
		SpawnRate = 25.f;
		SpawnRadius = 4500.f;
		ParticleScale = 0.5f;
		DriftSpeed = 120.f; // Fast upward (rising embers)
		ParticleLifetime = 5.f;
	}
	else if (BiomeKey == TEXT("VOID_REACH"))
	{
		// Void tendrils: sickly green-black, erratic, dense near ground
		ParticleColor = FLinearColor(0.2f, 0.35f, 0.15f, 0.5f);
		SpawnRate = 18.f;
		SpawnRadius = 5500.f;
		ParticleScale = 2.0f;
		DriftSpeed = -40.f; // Sinking tendrils
		ParticleLifetime = 8.f;
	}

	// Apply Niagara user parameters if component is valid
	if (NiagaraComp)
	{
		NiagaraComp->SetColorParameter(FName("ParticleColor"), ParticleColor);
		NiagaraComp->SetFloatParameter(FName("SpawnRate"), SpawnRate);
		NiagaraComp->SetFloatParameter(FName("SpawnRadius"), SpawnRadius);
		NiagaraComp->SetFloatParameter(FName("ParticleScale"), ParticleScale);
		NiagaraComp->SetFloatParameter(FName("DriftSpeed"), DriftSpeed);
		NiagaraComp->SetFloatParameter(FName("ParticleLifetime"), ParticleLifetime);
	}

	UE_LOG(LogTemp, Log, TEXT("TartariaBiomeParticles: Configured for %s "
		"(color=[%.1f,%.1f,%.1f], rate=%.0f, radius=%.0f, drift=%.0f)"),
		*BiomeKey,
		ParticleColor.R, ParticleColor.G, ParticleColor.B,
		SpawnRate, SpawnRadius, DriftSpeed);
}

void ATartariaBiomeParticles::ActivateParticles()
{
	if (!NiagaraComp) return;

	// Set the Niagara system asset if assigned
	if (NiagaraSystemAsset)
	{
		NiagaraComp->SetAsset(NiagaraSystemAsset);
	}

	// Apply biome parameters
	if (NiagaraComp->GetAsset())
	{
		NiagaraComp->SetColorParameter(FName("ParticleColor"), ParticleColor);
		NiagaraComp->SetFloatParameter(FName("SpawnRate"), SpawnRate);
		NiagaraComp->SetFloatParameter(FName("SpawnRadius"), SpawnRadius);
		NiagaraComp->SetFloatParameter(FName("ParticleScale"), ParticleScale);
		NiagaraComp->SetFloatParameter(FName("DriftSpeed"), DriftSpeed);
		NiagaraComp->SetFloatParameter(FName("ParticleLifetime"), ParticleLifetime);

		NiagaraComp->Activate(true);
		bParticlesActive = true;

		UE_LOG(LogTemp, Log, TEXT("TartariaBiomeParticles: Activated for %s"), *BiomeKey);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("TartariaBiomeParticles: %s — no Niagara asset set, particles inactive."),
			*BiomeKey);
	}
}

void ATartariaBiomeParticles::DeactivateParticles()
{
	if (NiagaraComp)
	{
		NiagaraComp->Deactivate();
	}
	bParticlesActive = false;
}

void ATartariaBiomeParticles::FollowPlayer()
{
	// Move particle emitter to track the player's XY position (keep Z at biome ground level)
	// This keeps atmospheric particles always around the player within the biome
	UWorld* World = GetWorld();
	if (!World) return;

	APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!Player) return;

	FVector PlayerLoc = Player->GetActorLocation();
	FVector MyLoc = GetActorLocation();

	// Only reposition if player has moved significantly (>1000 UU = 10m)
	float DistSq = FVector::DistSquared(FVector(PlayerLoc.X, PlayerLoc.Y, 0.f),
	                                      FVector(MyLoc.X, MyLoc.Y, 0.f));
	if (DistSq > 1000000.f) // 1000^2
	{
		// Smoothly lerp toward player position (keep Z)
		FVector NewLoc = FMath::Lerp(MyLoc,
			FVector(PlayerLoc.X, PlayerLoc.Y, MyLoc.Z), 0.3f);
		SetActorLocation(NewLoc);
	}
}
