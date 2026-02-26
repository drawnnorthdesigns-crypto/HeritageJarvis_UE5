#include "TartariaBiomeVolume.h"
#include "Components/SphereComponent.h"
#include "Components/PostProcessComponent.h"
#include "GameFramework/Character.h"

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
	if (ACharacter* PlayerChar = Cast<ACharacter>(OtherActor))
	{
		UE_LOG(LogTemp, Log, TEXT("TartariaBiomeVolume: Player entered %s biome"), *BiomeKey);
	}
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
