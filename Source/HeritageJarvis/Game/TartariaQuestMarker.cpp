#include "TartariaQuestMarker.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"

ATartariaQuestMarker::ATartariaQuestMarker()
{
	PrimaryActorTick.bCanEverTick = true;

	// Marker mesh (assign diamond/crystal shape in editor)
	MarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerMesh"));
	RootComponent = MarkerMesh;
	MarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Glowing light indicator
	MarkerLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MarkerLight"));
	MarkerLight->SetupAttachment(RootComponent);
	MarkerLight->SetRelativeLocation(FVector(0.f, 0.f, 20.f));
	MarkerLight->SetIntensity(5000.f);
	MarkerLight->SetLightColor(FLinearColor(1.0f, 0.9f, 0.3f)); // Gold glow
	MarkerLight->SetAttenuationRadius(500.f);
}

void ATartariaQuestMarker::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bActive) return;

	// Bob animation
	BobTimer += DeltaTime * BobSpeed;
	float BobOffset = FMath::Sin(BobTimer * 2.f * PI) * BobAmplitude;

	if (InitialLocation.IsZero())
		InitialLocation = GetActorLocation();

	FVector NewLocation = InitialLocation;
	NewLocation.Z += BobOffset;
	SetActorLocation(NewLocation);

	// Slow rotation
	FRotator CurrentRotation = GetActorRotation();
	CurrentRotation.Yaw += 45.f * DeltaTime; // 45 degrees per second
	SetActorRotation(CurrentRotation);
}

void ATartariaQuestMarker::ActivateQuest()
{
	bActive = true;
	SetActorHiddenInGame(false);
	if (MarkerLight) MarkerLight->SetVisibility(true);
	OnQuestActivated();

	UE_LOG(LogTemp, Log, TEXT("TartariaQuestMarker: Quest '%s' activated"), *QuestTitle);
}

void ATartariaQuestMarker::CompleteQuest()
{
	bActive = false;
	SetActorHiddenInGame(true);
	if (MarkerLight) MarkerLight->SetVisibility(false);
	OnQuestCompleted();

	UE_LOG(LogTemp, Log, TEXT("TartariaQuestMarker: Quest '%s' completed"), *QuestTitle);
}
