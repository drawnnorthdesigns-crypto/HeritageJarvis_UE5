#include "TartariaBiomeVolume.h"
#include "TartariaCharacter.h"
#include "Core/HJGameInstance.h"
#include "Core/HJApiClient.h"
#include "Components/SphereComponent.h"
#include "Components/PostProcessComponent.h"
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
