#include "TartariaPatentRegistry.h"
#include "TartariaGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMesh.h"

ATartariaPatentRegistry::ATartariaPatentRegistry()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.033f;  // 30 fps for smooth rotation

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// Pedestal — dark stone cylinder base
	PedestalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pedestal"));
	PedestalMesh->SetupAttachment(RootComponent);
	UStaticMesh* CylMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylMesh) PedestalMesh->SetStaticMesh(CylMesh);
	PedestalMesh->SetRelativeScale3D(FVector(1.2f, 1.2f, 0.3f));
	PedestalMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PedestalMesh->SetCollisionResponseToAllChannels(ECR_Block);

	// Holographic projector glow — blue-white light
	HoloGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("HoloGlow"));
	HoloGlow->SetupAttachment(RootComponent);
	HoloGlow->SetRelativeLocation(FVector(0.f, 0.f, 200.f));
	HoloGlow->SetLightColor(FLinearColor(0.3f, 0.5f, 1.0f));
	HoloGlow->SetIntensity(5000.f);
	HoloGlow->SetAttenuationRadius(400.f);
	HoloGlow->CastShadows = false;
}

void ATartariaPatentRegistry::BeginPlay()
{
	Super::BeginPlay();

	// Apply pedestal material
	UMaterialInstanceDynamic* PedMat = PedestalMesh->CreateAndSetMaterialInstanceDynamic(0);
	if (PedMat)
	{
		PedMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.15f, 0.12f, 0.1f));
	}

	BuildPlaques();
}

void ATartariaPatentRegistry::BuildPlaques()
{
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!CubeMesh) return;

	for (int32 i = 0; i < MaxPlaques; i++)
	{
		UStaticMeshComponent* Plaque = NewObject<UStaticMeshComponent>(this,
			*FString::Printf(TEXT("Plaque_%d"), i));
		Plaque->SetupAttachment(RootComponent);
		Plaque->SetStaticMesh(CubeMesh);

		// Arrange in a circle around the pedestal
		float Angle = (static_cast<float>(i) / MaxPlaques) * 360.f;
		float AngleRad = FMath::DegreesToRadians(Angle);
		float Radius = 120.f;
		float Height = 100.f + (i % 3) * 40.f;  // Stagger heights

		Plaque->SetRelativeLocation(FVector(
			FMath::Cos(AngleRad) * Radius,
			FMath::Sin(AngleRad) * Radius,
			Height
		));
		Plaque->SetRelativeScale3D(FVector(0.25f, 0.15f, 0.005f));  // Thin plaque
		Plaque->SetRelativeRotation(FRotator(0.f, Angle + 90.f, 0.f));
		Plaque->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Plaque->CastShadow = false;

		// Holographic emissive material
		UMaterialInstanceDynamic* Mat = Plaque->CreateAndSetMaterialInstanceDynamic(0);
		if (Mat)
		{
			// Alternate between gold and blue tint
			FLinearColor PlaqueColor = (i % 2 == 0)
				? FLinearColor(0.7f, 0.55f, 0.2f)   // Gold
				: FLinearColor(0.3f, 0.5f, 0.9f);   // Blue
			Mat->SetVectorParameterValue(TEXT("Color"), PlaqueColor);
			Mat->SetScalarParameterValue(TEXT("Emissive"), 4.0f);
		}

		// Start hidden — revealed by UpdatePatentCount
		Plaque->SetVisibility(false);
		Plaque->RegisterComponent();
		PlaqueMeshes.Add(Plaque);
	}
}

void ATartariaPatentRegistry::UpdatePatentCount(int32 NewCount)
{
	CurrentPlaques = FMath::Clamp(NewCount, 0, MaxPlaques);

	for (int32 i = 0; i < PlaqueMeshes.Num(); i++)
	{
		if (PlaqueMeshes[i])
			PlaqueMeshes[i]->SetVisibility(i < CurrentPlaques);
	}

	// Scale glow with patent count
	float GlowScale = FMath::Clamp(static_cast<float>(CurrentPlaques) / 6.f, 0.5f, 3.0f);
	HoloGlow->SetIntensity(3000.f * GlowScale);

	UE_LOG(LogTemp, Log, TEXT("TartariaPatentRegistry: Showing %d/%d patent plaques"),
		CurrentPlaques, MaxPlaques);
}

void ATartariaPatentRegistry::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	AnimTime += DeltaTime;

	// Slowly rotate visible plaques and bob them up/down
	for (int32 i = 0; i < PlaqueMeshes.Num(); i++)
	{
		UStaticMeshComponent* Plaque = PlaqueMeshes[i];
		if (!Plaque || !Plaque->IsVisible()) continue;

		float Angle = (static_cast<float>(i) / MaxPlaques) * 360.f;
		float RotSpeed = 15.f;  // 15 deg/sec orbit
		float CurrentAngle = Angle + AnimTime * RotSpeed;
		float AngleRad = FMath::DegreesToRadians(CurrentAngle);
		float Radius = 120.f;
		float BaseHeight = 100.f + (i % 3) * 40.f;
		float Bob = FMath::Sin(AnimTime * 2.f + i * 0.8f) * 8.f;

		Plaque->SetRelativeLocation(FVector(
			FMath::Cos(AngleRad) * Radius,
			FMath::Sin(AngleRad) * Radius,
			BaseHeight + Bob
		));
		// Face outward
		Plaque->SetRelativeRotation(FRotator(0.f, CurrentAngle + 90.f, 0.f));
	}

	// Pulse the glow
	float GlowPulse = 1.0f + FMath::Sin(AnimTime * 3.f) * 0.15f;
	HoloGlow->SetIntensity(HoloGlow->Intensity * GlowPulse);
}

void ATartariaPatentRegistry::OnInteract_Implementation(APlayerController* Interactor)
{
	// Open the project archive in CEF dashboard
	if (ATartariaGameMode* GM =
	    Cast<ATartariaGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->OpenDashboardToRoute(TEXT("/projects"));
	}
}

FString ATartariaPatentRegistry::GetInteractPrompt_Implementation() const
{
	return FString::Printf(TEXT("Patent Registry (%d patents filed)"), CurrentPlaques);
}
