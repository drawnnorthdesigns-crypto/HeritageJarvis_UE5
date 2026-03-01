#include "TartariaForgeBuilding.h"
#include "TartariaGameMode.h"
#include "TartariaTerminal.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "ProceduralMeshComponent.h"
#include "Core/HJMeshLoader.h"
#include "Core/HJGameInstance.h"
#include "Core/HJApiClient.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Async/Async.h"
#include "EngineUtils.h"

ATartariaForgeBuilding::ATartariaForgeBuilding()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.033f; // ~30 fps for visual effects

	// Base mesh — cube primitive as foundation (will be augmented by SetupBuildingVisuals)
	BuildingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BuildingMesh"));
	RootComponent = BuildingMesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(
		TEXT("/Engine/BasicShapes/Cube"));
	if (MeshFinder.Succeeded())
		BuildingMesh->SetStaticMesh(MeshFinder.Object);
	BuildingMesh->SetWorldScale3D(FVector(3.0f, 3.0f, 5.0f));

	// Ambient light for the building
	BuildingLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BuildingLight"));
	BuildingLight->SetupAttachment(RootComponent);
	BuildingLight->SetRelativeLocation(FVector(0.f, 0.f, 300.f));
	BuildingLight->SetIntensity(5000.f);
	BuildingLight->SetLightColor(FLinearColor(1.0f, 0.85f, 0.5f)); // Warm amber
	BuildingLight->SetAttenuationRadius(800.f);

	// Chimney flicker light — only visible when forge is active
	ChimneyFlickerLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ChimneyFlickerLight"));
	ChimneyFlickerLight->SetupAttachment(RootComponent);
	ChimneyFlickerLight->SetRelativeLocation(FVector(120.f, 0.f, 500.f));
	ChimneyFlickerLight->SetIntensity(0.f); // Off by default
	ChimneyFlickerLight->SetLightColor(FLinearColor(1.0f, 0.4f, 0.05f)); // Deep orange fire
	ChimneyFlickerLight->SetAttenuationRadius(600.f);

	BaseForgeColor = FLinearColor(1.0f, 0.55f, 0.15f);

	// ── Display Pedestal: stone platform + procedural mesh for forged models ──

	// Pedestal base — cylinder in front of the building
	PedestalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PedestalMesh"));
	PedestalMesh->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PedestalFinder(
		TEXT("/Engine/BasicShapes/Cylinder"));
	if (PedestalFinder.Succeeded())
		PedestalMesh->SetStaticMesh(PedestalFinder.Object);
	PedestalMesh->SetRelativeLocation(FVector(-300.f, 0.f, -100.f));
	PedestalMesh->SetRelativeScale3D(FVector(1.2f, 1.2f, 0.15f));
	PedestalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Display model — procedural mesh mounted on top of pedestal
	DisplayModelMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("DisplayModelMesh"));
	DisplayModelMesh->SetupAttachment(PedestalMesh);
	DisplayModelMesh->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	DisplayModelMesh->SetRelativeScale3D(FVector(0.01f, 0.01f, 0.01f)); // mm→cm
	DisplayModelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DisplayModelMesh->SetVisibility(false); // Hidden until model loads

	// Mesh loader component
	MeshLoader = CreateDefaultSubobject<UHJMeshLoader>(TEXT("MeshLoader"));
	MeshLoader->TargetMeshComponent = DisplayModelMesh;
	MeshLoader->bEnableCollision = false;
	MeshLoader->LODLevel = TEXT("lod2"); // Medium detail for display

	// Pedestal spotlight — dramatic top-down illumination
	PedestalLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("PedestalLight"));
	PedestalLight->SetupAttachment(PedestalMesh);
	PedestalLight->SetRelativeLocation(FVector(0.f, 0.f, 300.f));
	PedestalLight->SetIntensity(0.f); // Off until model loads
	PedestalLight->SetLightColor(FLinearColor(0.9f, 0.95f, 1.0f)); // Cool white
	PedestalLight->SetAttenuationRadius(400.f);
}

// -------------------------------------------------------
// Compound Architecture Builder
// -------------------------------------------------------

UStaticMeshComponent* ATartariaForgeBuilding::AddPrimitiveMesh(
	const FName& Name, const TCHAR* ShapePath,
	const FVector& RelLoc, const FRotator& RelRot, const FVector& Scale,
	const FLinearColor& Color)
{
	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this, Name);
	if (!Mesh) return nullptr;

	Mesh->SetupAttachment(RootComponent);
	Mesh->RegisterComponent();

	UStaticMesh* Shape = LoadObject<UStaticMesh>(nullptr, ShapePath);
	if (Shape)
		Mesh->SetStaticMesh(Shape);

	Mesh->SetRelativeLocation(RelLoc);
	Mesh->SetRelativeRotation(RelRot);
	Mesh->SetRelativeScale3D(Scale);

	// Apply solid color via dynamic material instance
	UMaterialInstanceDynamic* DynMat = Mesh->CreateAndSetMaterialInstanceDynamic(0);
	if (DynMat)
		DynMat->SetVectorParameterValue(TEXT("Color"), Color);

	return Mesh;
}

UStaticMeshComponent* ATartariaForgeBuilding::AddWindowStrip(
	const FName& Name,
	const FVector& RelLoc, const FRotator& RelRot, const FVector& Scale,
	const FLinearColor& EmissiveColor)
{
	UStaticMeshComponent* Strip = NewObject<UStaticMeshComponent>(this, Name);
	if (!Strip) return nullptr;

	Strip->SetupAttachment(RootComponent);
	Strip->RegisterComponent();

	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane"));
	if (Plane)
		Strip->SetStaticMesh(Plane);

	Strip->SetRelativeLocation(RelLoc);
	Strip->SetRelativeRotation(RelRot);
	Strip->SetRelativeScale3D(Scale);
	Strip->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	UMaterialInstanceDynamic* DynMat = Strip->CreateAndSetMaterialInstanceDynamic(0);
	if (DynMat)
	{
		DynMat->SetVectorParameterValue(TEXT("Color"), EmissiveColor);
		// Boost emissive so windows glow at night
		DynMat->SetScalarParameterValue(TEXT("Emissive"), 3.0f);
		// Track for dynamic forge pulse
		WindowMaterials.Add(DynMat);
	}

	return Strip;
}

void ATartariaForgeBuilding::SetupBuildingVisuals(float ScaleMultiplier)
{
	const float S = ScaleMultiplier;
	const TCHAR* Cube = TEXT("/Engine/BasicShapes/Cube");
	const TCHAR* Cylinder = TEXT("/Engine/BasicShapes/Cylinder");
	const TCHAR* Sphere = TEXT("/Engine/BasicShapes/Sphere");

	switch (BuildingType)
	{
	// ── FORGE: Wide base + tall chimney + angled roof slab ──────────
	case ETartariaBuildingType::Forge:
	{
		// Base already set (cube 3x3x5)
		BuildingMesh->SetRelativeScale3D(FVector(4.f, 3.f, 3.f) * S);

		const FLinearColor Main(1.0f, 0.5f, 0.0f);
		const FLinearColor Dark(0.6f, 0.3f, 0.05f);
		const FLinearColor Glow(1.0f, 0.6f, 0.15f);

		// Chimney — tall cylinder on right side
		AddPrimitiveMesh(FName("Chimney"), Cylinder,
			FVector(120.f * S, 0.f, 350.f * S), FRotator::ZeroRotator,
			FVector(0.8f, 0.8f, 4.f) * S, Dark);

		// Roof slab — angled cube
		AddPrimitiveMesh(FName("Roof"), Cube,
			FVector(0.f, 0.f, 220.f * S), FRotator(10.f, 0.f, 0.f),
			FVector(4.5f, 3.5f, 0.3f) * S, Dark);

		// Anvil platform — low wide cube in front
		AddPrimitiveMesh(FName("AnvilPlatform"), Cube,
			FVector(-200.f * S, 0.f, -100.f * S), FRotator::ZeroRotator,
			FVector(1.5f, 2.f, 0.5f) * S, Dark);

		// Window strips — emissive slits in the walls
		AddWindowStrip(FName("Window1"),
			FVector(-201.f * S, 40.f * S, 80.f * S), FRotator(0.f, 90.f, 90.f),
			FVector(0.5f, 0.15f, 1.f) * S, Glow);
		AddWindowStrip(FName("Window2"),
			FVector(-201.f * S, -40.f * S, 80.f * S), FRotator(0.f, 90.f, 90.f),
			FVector(0.5f, 0.15f, 1.f) * S, Glow);

		// Forge glow light — warm orange, extra bright
		BuildingLight->SetLightColor(FLinearColor(1.0f, 0.55f, 0.15f));
		BuildingLight->SetIntensity(8000.f);
		BuildingLight->SetRelativeLocation(FVector(0.f, 0.f, 150.f * S));

		break;
	}

	// ── SCRIPTORIUM: Tall tower + dome on top + flanking wings ──────
	case ETartariaBuildingType::Scriptorium:
	{
		BuildingMesh->SetRelativeScale3D(FVector(3.f, 3.f, 5.f) * S);

		const FLinearColor Main(0.2f, 0.3f, 0.8f);
		const FLinearColor Light(0.35f, 0.45f, 0.85f);
		const FLinearColor Glow(0.4f, 0.6f, 1.0f);

		// Dome — hemisphere on top of tower
		AddPrimitiveMesh(FName("Dome"), Sphere,
			FVector(0.f, 0.f, 350.f * S), FRotator::ZeroRotator,
			FVector(3.5f, 3.5f, 2.f) * S, Light);

		// Left wing — lower rectangle
		AddPrimitiveMesh(FName("LeftWing"), Cube,
			FVector(0.f, 250.f * S, -80.f * S), FRotator::ZeroRotator,
			FVector(2.f, 2.f, 2.f) * S, Main);

		// Right wing — lower rectangle
		AddPrimitiveMesh(FName("RightWing"), Cube,
			FVector(0.f, -250.f * S, -80.f * S), FRotator::ZeroRotator,
			FVector(2.f, 2.f, 2.f) * S, Main);

		// Entrance columns — two thin cylinders
		AddPrimitiveMesh(FName("ColumnL"), Cylinder,
			FVector(-180.f * S, 80.f * S, -20.f * S), FRotator::ZeroRotator,
			FVector(0.3f, 0.3f, 3.f) * S, Light);
		AddPrimitiveMesh(FName("ColumnR"), Cylinder,
			FVector(-180.f * S, -80.f * S, -20.f * S), FRotator::ZeroRotator,
			FVector(0.3f, 0.3f, 3.f) * S, Light);

		// Window strips — blue glow
		AddWindowStrip(FName("Window1"),
			FVector(-151.f * S, 0.f, 150.f * S), FRotator(0.f, 90.f, 90.f),
			FVector(1.2f, 0.1f, 1.f) * S, Glow);

		BuildingLight->SetLightColor(FLinearColor(0.4f, 0.5f, 1.0f));
		BuildingLight->SetRelativeLocation(FVector(0.f, 0.f, 400.f * S));

		break;
	}

	// ── TREASURY: Squat fortified cube + corner pillars + vault door ─
	case ETartariaBuildingType::Treasury:
	{
		BuildingMesh->SetRelativeScale3D(FVector(4.f, 4.f, 3.f) * S);

		const FLinearColor Main(0.9f, 0.8f, 0.2f);
		const FLinearColor Dark(0.6f, 0.5f, 0.15f);
		const FLinearColor Gold(1.0f, 0.85f, 0.3f);

		// Corner pillars (4x)
		const float PillarOff = 200.f * S;
		AddPrimitiveMesh(FName("Pillar_FL"), Cylinder,
			FVector(-PillarOff, PillarOff, 0.f), FRotator::ZeroRotator,
			FVector(0.6f, 0.6f, 3.5f) * S, Dark);
		AddPrimitiveMesh(FName("Pillar_FR"), Cylinder,
			FVector(-PillarOff, -PillarOff, 0.f), FRotator::ZeroRotator,
			FVector(0.6f, 0.6f, 3.5f) * S, Dark);
		AddPrimitiveMesh(FName("Pillar_BL"), Cylinder,
			FVector(PillarOff, PillarOff, 0.f), FRotator::ZeroRotator,
			FVector(0.6f, 0.6f, 3.5f) * S, Dark);
		AddPrimitiveMesh(FName("Pillar_BR"), Cylinder,
			FVector(PillarOff, -PillarOff, 0.f), FRotator::ZeroRotator,
			FVector(0.6f, 0.6f, 3.5f) * S, Dark);

		// Crown/parapet — thin wide cube on top
		AddPrimitiveMesh(FName("Parapet"), Cube,
			FVector(0.f, 0.f, 200.f * S), FRotator::ZeroRotator,
			FVector(4.5f, 4.5f, 0.4f) * S, Dark);

		// Vault door — thick slab in front
		AddPrimitiveMesh(FName("VaultDoor"), Cube,
			FVector(-210.f * S, 0.f, -30.f * S), FRotator::ZeroRotator,
			FVector(0.2f, 1.5f, 2.f) * S, FLinearColor(0.4f, 0.35f, 0.1f));

		// Gold window glow
		AddWindowStrip(FName("Window1"),
			FVector(-211.f * S, 60.f * S, 80.f * S), FRotator(0.f, 90.f, 90.f),
			FVector(0.3f, 0.1f, 1.f) * S, Gold);
		AddWindowStrip(FName("Window2"),
			FVector(-211.f * S, -60.f * S, 80.f * S), FRotator(0.f, 90.f, 90.f),
			FVector(0.3f, 0.1f, 1.f) * S, Gold);

		BuildingLight->SetLightColor(FLinearColor(1.0f, 0.9f, 0.4f));
		BuildingLight->SetRelativeLocation(FVector(0.f, 0.f, 250.f * S));

		break;
	}

	// ── BARRACKS: Long hall + watchtower + rampart slabs ────────────
	case ETartariaBuildingType::Barracks:
	{
		BuildingMesh->SetRelativeScale3D(FVector(6.f, 3.f, 2.5f) * S);

		const FLinearColor Main(0.3f, 0.5f, 0.2f);
		const FLinearColor Stone(0.35f, 0.32f, 0.25f);
		const FLinearColor Glow(0.5f, 0.8f, 0.3f);

		// Watchtower — tall cylinder on one end
		AddPrimitiveMesh(FName("Watchtower"), Cylinder,
			FVector(250.f * S, 0.f, 150.f * S), FRotator::ZeroRotator,
			FVector(1.0f, 1.0f, 4.f) * S, Stone);

		// Watchtower cap — small sphere
		AddPrimitiveMesh(FName("TowerCap"), Sphere,
			FVector(250.f * S, 0.f, 450.f * S), FRotator::ZeroRotator,
			FVector(1.2f, 1.2f, 0.6f) * S, Main);

		// Rampart wall — long low slab extending from building
		AddPrimitiveMesh(FName("Rampart"), Cube,
			FVector(0.f, 200.f * S, -80.f * S), FRotator::ZeroRotator,
			FVector(7.f, 0.3f, 1.5f) * S, Stone);

		// Training dummy area — low platform
		AddPrimitiveMesh(FName("TrainPad"), Cube,
			FVector(-300.f * S, 0.f, -120.f * S), FRotator::ZeroRotator,
			FVector(2.f, 2.f, 0.2f) * S, Stone);

		// Window strips — green tactical glow
		AddWindowStrip(FName("Window1"),
			FVector(0.f, -151.f * S, 50.f * S), FRotator(0.f, 0.f, 90.f),
			FVector(2.f, 0.1f, 1.f) * S, Glow);

		BuildingLight->SetLightColor(FLinearColor(0.5f, 0.7f, 0.3f));
		BuildingLight->SetRelativeLocation(FVector(250.f * S, 0.f, 500.f * S));
		BuildingLight->SetAttenuationRadius(1200.f);

		break;
	}

	// ── LAB: Central cylinder + ring of smaller cubes + antenna ─────
	case ETartariaBuildingType::Lab:
	{
		BuildingMesh->SetRelativeScale3D(FVector(3.f, 3.f, 4.f) * S);

		const FLinearColor Main(0.4f, 0.1f, 0.6f);
		const FLinearColor Light(0.55f, 0.25f, 0.75f);
		const FLinearColor Glow(0.6f, 0.2f, 1.0f);

		// Replace base shape with cylinder for lab
		UStaticMesh* CylMesh = LoadObject<UStaticMesh>(nullptr, Cylinder);
		if (CylMesh)
			BuildingMesh->SetStaticMesh(CylMesh);

		// Ring of experiment pods (6 small cubes around perimeter)
		const int32 PodCount = 6;
		const float PodRadius = 200.f * S;
		for (int32 i = 0; i < PodCount; ++i)
		{
			float Angle = (float)i / PodCount * 2.f * PI;
			FVector Offset(FMath::Cos(Angle) * PodRadius, FMath::Sin(Angle) * PodRadius, -60.f * S);

			AddPrimitiveMesh(
				*FString::Printf(TEXT("Pod_%d"), i), Cube,
				Offset, FRotator(0.f, FMath::RadiansToDegrees(Angle), 0.f),
				FVector(0.8f, 0.8f, 1.5f) * S, Main);
		}

		// Antenna spire — tall thin cylinder on top
		AddPrimitiveMesh(FName("Antenna"), Cylinder,
			FVector(0.f, 0.f, 350.f * S), FRotator::ZeroRotator,
			FVector(0.15f, 0.15f, 3.f) * S, Light);

		// Antenna tip — small glowing sphere
		AddPrimitiveMesh(FName("AntennaTip"), Sphere,
			FVector(0.f, 0.f, 550.f * S), FRotator::ZeroRotator,
			FVector(0.4f, 0.4f, 0.4f) * S, Glow);

		// Purple glow windows
		AddWindowStrip(FName("Window1"),
			FVector(-151.f * S, 0.f, 100.f * S), FRotator(0.f, 90.f, 90.f),
			FVector(0.5f, 0.12f, 1.f) * S, Glow);
		AddWindowStrip(FName("Window2"),
			FVector(0.f, -151.f * S, 100.f * S), FRotator(0.f, 0.f, 90.f),
			FVector(0.5f, 0.12f, 1.f) * S, Glow);

		BuildingLight->SetLightColor(FLinearColor(0.5f, 0.2f, 0.9f));
		BuildingLight->SetIntensity(6000.f);
		BuildingLight->SetRelativeLocation(FVector(0.f, 0.f, 400.f * S));

		// Second light at antenna tip for dramatic void glow
		UPointLightComponent* TipLight = NewObject<UPointLightComponent>(this, TEXT("AntennaTipLight"));
		if (TipLight)
		{
			TipLight->SetupAttachment(RootComponent);
			TipLight->RegisterComponent();
			TipLight->SetRelativeLocation(FVector(0.f, 0.f, 550.f * S));
			TipLight->SetIntensity(4000.f);
			TipLight->SetLightColor(FLinearColor(0.6f, 0.2f, 1.0f));
			TipLight->SetAttenuationRadius(600.f);
		}

		break;
	}
	}
}

// -------------------------------------------------------
// Dynamic Forge Effects
// -------------------------------------------------------

void ATartariaForgeBuilding::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Construction animation
	if (bIsConstructing && ConstructionProgress < 1.0f)
	{
		ConstructionTimer += DeltaTime;
		ConstructionProgress = FMath::Clamp(ConstructionTimer / ConstructionDuration, 0.0f, 1.0f);

		// Scale building up from ground
		float ScaleY = FMath::InterpEaseOut(0.1f, 1.0f, ConstructionProgress, 2.0f);
		BuildingMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, ScaleY));

		// Glow intensifies during construction
		if (BuildingLight)
		{
			BuildingLight->SetIntensity(BaseForgeIntensity * 3.0f * (1.0f - ConstructionProgress) + BaseForgeIntensity * ConstructionProgress);
		}

		if (ConstructionProgress >= 1.0f)
		{
			bIsConstructing = false;
			BuildingMesh->SetRelativeScale3D(FVector(1.0f));
			// Flash on completion
			if (BuildingLight)
			{
				BuildingLight->SetIntensity(BaseForgeIntensity * 5.0f);
			}
		}
	}

	// Only run forge effects for Forge-type buildings
	if (BuildingType != ETartariaBuildingType::Forge) return;

	// Slow turntable rotation for displayed model
	if (bModelDisplayed && DisplayModelMesh && DisplayModelMesh->IsVisible())
	{
		DisplayRotation += DeltaTime * 15.0f; // 15 deg/sec
		DisplayModelMesh->SetRelativeRotation(FRotator(0.f, DisplayRotation, 0.f));
	}

	GlowAccumulator += DeltaTime;

	if (bForgeActive)
	{
		// ── Active Forge: Pulsing glow + chimney flicker ──

		// Main light: sinusoidal intensity pulse (breathing fire)
		float Pulse = FMath::Sin(GlowAccumulator * 2.5f) * 0.3f + 1.0f;
		float ActiveIntensity = BaseForgeIntensity * Pulse;
		BuildingLight->SetIntensity(ActiveIntensity);

		// Subtle color shift toward white-hot at peak
		float ColorShift = FMath::Sin(GlowAccumulator * 1.8f) * 0.1f;
		FLinearColor PulseColor = FLinearColor(
			FMath::Clamp(BaseForgeColor.R + ColorShift, 0.f, 1.f),
			FMath::Clamp(BaseForgeColor.G + ColorShift * 0.5f, 0.f, 1.f),
			BaseForgeColor.B
		);
		BuildingLight->SetLightColor(PulseColor);

		// Chimney flicker: rapid random intensity (fire simulation)
		float Flicker = FMath::FRandRange(3000.f, 7000.f);
		// Add a slower wave for breathing effect
		Flicker *= (FMath::Sin(GlowAccumulator * 4.f) * 0.2f + 1.0f);
		ChimneyFlickerLight->SetIntensity(Flicker);

		// Window emissive pulse
		float EmissivePulse = FMath::Sin(GlowAccumulator * 2.0f) * 2.0f + 5.0f;
		for (int32 i = 0; i < WindowMaterials.Num(); ++i)
		{
			if (WindowMaterials[i])
			{
				WindowMaterials[i]->SetScalarParameterValue(TEXT("Emissive"), EmissivePulse);
			}
		}
	}
	else
	{
		// ── Idle Forge: Gentle ambient glow ──

		// Slow, subtle breathing
		float IdlePulse = FMath::Sin(GlowAccumulator * 0.5f) * 0.1f + 1.0f;
		BuildingLight->SetIntensity(IdleIntensity * IdlePulse);
		BuildingLight->SetLightColor(BaseForgeColor);

		// Chimney dark when idle
		ChimneyFlickerLight->SetIntensity(0.f);

		// Windows at low emissive
		for (int32 i = 0; i < WindowMaterials.Num(); ++i)
		{
			if (WindowMaterials[i])
			{
				WindowMaterials[i]->SetScalarParameterValue(TEXT("Emissive"), 2.0f);
			}
		}
	}

	// -- Torch Flicker --
	FlickerPhase += DeltaTime * 8.f;
	if (BuildingLight)
	{
		float BaseIntensity = bForgeActive ? 8000.f : 4000.f;
		float Flicker = FMath::Sin(FlickerPhase) * 0.15f
		              + FMath::Sin(FlickerPhase * 2.7f) * 0.08f
		              + FMath::Sin(FlickerPhase * 5.3f) * 0.04f;
		BuildingLight->SetIntensity(BaseIntensity * (1.f + Flicker));
	}
}

void ATartariaForgeBuilding::SetForgeActive(bool bActive, const FString& StageName)
{
	bool bWasActive = bForgeActive;
	bForgeActive = bActive;
	CurrentForgeStage = StageName;

	if (bActive && !bWasActive)
	{
		// Forge just activated — reset glow accumulator for fresh pulse
		GlowAccumulator = 0.f;
		UE_LOG(LogTemp, Log, TEXT("Forge '%s' activated — pipeline stage: %s"), *BuildingName, *StageName);
	}
	else if (!bActive && bWasActive)
	{
		UE_LOG(LogTemp, Log, TEXT("Forge '%s' deactivated"), *BuildingName);
	}
}

void ATartariaForgeBuilding::OnStageTransition(const FString& NewStage, int32 StageIndex)
{
	FString OldStage = CurrentForgeStage;
	CurrentForgeStage = NewStage;

	UE_LOG(LogTemp, Log, TEXT("Forge '%s' stage transition: %s -> %s (index %d)"),
		*BuildingName, *OldStage, *NewStage, StageIndex);

	// Intensity spike on transition (anvil strike visual)
	if (BuildingLight)
	{
		BuildingLight->SetIntensity(BaseForgeIntensity * 2.5f);
		// The tick will bring it back down smoothly via the pulse
	}

	// Chimney burst on transition
	if (ChimneyFlickerLight)
	{
		ChimneyFlickerLight->SetIntensity(12000.f);
	}

	// Fire Blueprint event for sound/particle hooks
	OnForgeStageChanged(NewStage, StageIndex);
}

void ATartariaForgeBuilding::SetProductionActive(bool bActive)
{
	if (bActive == bProductionActive) return;
	bProductionActive = bActive;

	if (bActive)
	{
		// Double building light intensity for active production zone
		if (BuildingLight)
		{
			BuildingLight->SetIntensity(BuildingLight->Intensity * 2.0f);
		}
		// Brighten chimney to show economic activity even when pipeline is idle
		if (ChimneyFlickerLight)
		{
			ChimneyFlickerLight->SetIntensity(
				FMath::Max(ChimneyFlickerLight->Intensity, 2000.f));
		}
		UE_LOG(LogTemp, Log, TEXT("Forge '%s': Production zone ACTIVE — lights boosted"),
			*BuildingName);
	}
	else
	{
		// Return to baseline idle intensity
		if (BuildingLight)
		{
			BuildingLight->SetIntensity(IdleIntensity);
		}
		if (ChimneyFlickerLight && !bForgeActive)
		{
			ChimneyFlickerLight->SetIntensity(0.f);
		}
		UE_LOG(LogTemp, Log, TEXT("Forge '%s': Production zone IDLE — lights reset"),
			*BuildingName);
	}
}

void ATartariaForgeBuilding::StartConstruction()
{
	bIsConstructing = true;
	ConstructionProgress = 0.0f;
	ConstructionTimer = 0.0f;
	BuildingMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.1f));
}

void ATartariaForgeBuilding::OnInteract_Implementation(APlayerController* Interactor)
{
	UE_LOG(LogTemp, Log, TEXT("TartariaForgeBuilding: Player interacted with %s (Level %d)"),
		*BuildingName, BuildingLevel);

	// Fire Blueprint event for custom UI handling
	OnBuildingInteracted(Interactor);

	// Try to find a nearby terminal and activate its 3D screen (diegetic UI)
	UWorld* World = GetWorld();
	if (World)
	{
		ATartariaTerminal* NearestTerminal = nullptr;
		float NearestDist = 50000.f; // 500m search radius

		for (TActorIterator<ATartariaTerminal> It(World); It; ++It)
		{
			ATartariaTerminal* Terminal = *It;
			if (Terminal->TerminalType == BuildingType)
			{
				float Dist = FVector::Dist(GetActorLocation(), Terminal->GetActorLocation());
				if (Dist < NearestDist)
				{
					NearestDist = Dist;
					NearestTerminal = Terminal;
				}
			}
		}

		if (NearestTerminal)
		{
			NearestTerminal->ActivateScreen(Interactor);
			return;
		}
	}

	// Fallback: open flat CEF dashboard overlay
	ATartariaGameMode* GM = Cast<ATartariaGameMode>(
		UGameplayStatics::GetGameMode(this));
	if (GM)
	{
		FString Route;
		switch (BuildingType)
		{
		case ETartariaBuildingType::Forge:
			// Forge opens the engineering console (Overwatch) for inspection
			Route = TEXT("/engineering");
			break;
		case ETartariaBuildingType::Scriptorium: Route = TEXT("/scriptorium"); break;
		case ETartariaBuildingType::Treasury:    Route = TEXT("/game#treasury"); break;
		case ETartariaBuildingType::Barracks:    Route = TEXT("/game#barracks"); break;
		case ETartariaBuildingType::Lab:         Route = TEXT("/game#lab"); break;
		}
		GM->OpenDashboardToRoute(Route);
	}
}

FString ATartariaForgeBuilding::GetInteractPrompt_Implementation() const
{
	if (BuildingType == ETartariaBuildingType::Forge && bModelDisplayed && !DisplayedProjectName.IsEmpty())
	{
		return FString::Printf(TEXT("Inspect %s — '%s' on display (Lv.%d)"),
			*BuildingName, *DisplayedProjectName, BuildingLevel);
	}
	return FString::Printf(TEXT("Enter %s (Lv.%d)"), *BuildingName, BuildingLevel);
}

// -------------------------------------------------------
// Display Pedestal — Latest Forged Model
// -------------------------------------------------------

void ATartariaForgeBuilding::LoadLatestForgedModel()
{
	if (BuildingType != ETartariaBuildingType::Forge) return;

	UHJGameInstance* GI = UHJGameInstance::Get(this);
	if (!GI || !GI->ApiClient) return;

	UE_LOG(LogTemp, Log, TEXT("Forge '%s': Fetching latest forged model..."), *BuildingName);

	TWeakObjectPtr<ATartariaForgeBuilding> WeakThis(this);

	FOnApiResponse CB;
	CB.BindLambda([WeakThis](bool bOk, const FString& RespBody)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, bOk, RespBody]()
		{
			ATartariaForgeBuilding* Self = WeakThis.Get();
			if (!Self) return;

			if (!bOk)
			{
				UE_LOG(LogTemp, Warning, TEXT("Forge '%s': Failed to fetch latest model info"),
					*Self->BuildingName);
				return;
			}

			// Parse response
			TSharedPtr<FJsonObject> Json;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RespBody);
			if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
				return;

			bool bFound = false;
			Json->TryGetBoolField(TEXT("found"), bFound);
			if (!bFound) return;

			FString ProjectId, ProjectName, ComponentName;
			Json->TryGetStringField(TEXT("project_id"), ProjectId);
			Json->TryGetStringField(TEXT("project_name"), ProjectName);
			Json->TryGetStringField(TEXT("component_name"), ComponentName);

			if (ProjectId.IsEmpty() || ComponentName.IsEmpty()) return;

			// Skip if already displaying this project
			if (ProjectId == Self->DisplayedProjectId) return;

			Self->DisplayedProjectId = ProjectId;
			Self->DisplayedProjectName = ProjectName;

			UE_LOG(LogTemp, Log, TEXT("Forge '%s': Loading model for '%s' (%s/%s)"),
				*Self->BuildingName, *ProjectName, *ProjectId, *ComponentName);

			// Subscribe to load completion
			if (Self->MeshLoader)
			{
				Self->MeshLoader->OnMeshLoaded.AddDynamic(
					Self, &ATartariaForgeBuilding::OnDisplayModelLoaded);
				Self->MeshLoader->LoadMeshFromProject(ProjectId, ComponentName);
			}
		});
	});

	GI->ApiClient->Get(TEXT("/api/engineering/latest-forged-model"), CB);
}

void ATartariaForgeBuilding::OnDisplayModelLoaded(bool bSuccess, const FString& StatusMessage)
{
	if (bSuccess)
	{
		bModelDisplayed = true;

		// Show the display mesh
		if (DisplayModelMesh)
			DisplayModelMesh->SetVisibility(true);

		// Light up pedestal with warm spotlight
		if (PedestalLight)
			PedestalLight->SetIntensity(6000.f);

		// Apply a golden Tartarian tint to the pedestal base
		if (PedestalMesh)
		{
			UMaterialInstanceDynamic* PedMat = PedestalMesh->CreateAndSetMaterialInstanceDynamic(0);
			if (PedMat)
			{
				PedMat->SetVectorParameterValue(TEXT("Color"),
					FLinearColor(0.35f, 0.3f, 0.2f)); // Dark stone
				PedMat->SetScalarParameterValue(TEXT("Emissive"), 0.5f);
			}
		}

		UE_LOG(LogTemp, Log, TEXT("Forge '%s': Model '%s' displayed on pedestal (%s)"),
			*BuildingName, *DisplayedProjectName, *StatusMessage);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Forge '%s': Failed to load display model: %s"),
			*BuildingName, *StatusMessage);
		bModelDisplayed = false;
		if (DisplayModelMesh) DisplayModelMesh->SetVisibility(false);
		if (PedestalLight) PedestalLight->SetIntensity(0.f);
	}

	// Unbind to avoid duplicate calls on next load
	if (MeshLoader)
		MeshLoader->OnMeshLoaded.RemoveDynamic(
			this, &ATartariaForgeBuilding::OnDisplayModelLoaded);
}
