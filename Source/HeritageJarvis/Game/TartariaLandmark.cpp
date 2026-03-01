#include "TartariaLandmark.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

ATartariaLandmark::ATartariaLandmark()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f; // 20 Hz — enough for smooth pulsing

	// Root scene component
	LandmarkRoot = CreateDefaultSubobject<USceneComponent>(TEXT("LandmarkRoot"));
	RootComponent = LandmarkRoot;

	// Primary landmark mesh — cube placeholder, replaced in ConfigureForBiome()
	LandmarkMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LandmarkMesh"));
	LandmarkMesh->SetupAttachment(RootComponent);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube"));
	if (CubeFinder.Succeeded())
	{
		LandmarkMesh->SetStaticMesh(CubeFinder.Object);
	}
	LandmarkMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Beacon light — extremely bright, long range, biome-colored
	BeaconLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BeaconLight"));
	BeaconLight->SetupAttachment(RootComponent);
	BeaconLight->SetRelativeLocation(FVector(0.f, 0.f, 2000.f)); // Top of default height
	BeaconLight->SetIntensity(50000.f);
	BeaconLight->SetAttenuationRadius(10000.f);
	BeaconLight->SetLightColor(FLinearColor(1.0f, 0.85f, 0.4f)); // Default warm gold
	BeaconLight->SetCastShadows(false); // Performance: no shadow from beacon
}

void ATartariaLandmark::BeginPlay()
{
	Super::BeginPlay();
	ConfigureForBiome();
}

void ATartariaLandmark::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ── Void Reach beacon pulsing: sinusoidal intensity + sphere bobbing ──
	if (bVoidPulseActive && BeaconLight)
	{
		VoidPulseAccumulator += DeltaTime;

		// Intensity oscillation: 0.3 Hz, +/- 50% of base intensity
		const float PulseWave = FMath::Sin(VoidPulseAccumulator * PI * 0.3f * 2.f);
		const float PulsedIntensity = BaseBeaconIntensity * (1.f + 0.5f * PulseWave);
		BeaconLight->SetIntensity(PulsedIntensity);

		// Floating sphere bobbing: 0.2 Hz vertical oscillation, +/- 30 UU
		if (VoidFloatingSphere)
		{
			const float BobWave = FMath::Sin(VoidPulseAccumulator * PI * 0.2f * 2.f);
			FVector Loc = VoidFloatingSphere->GetRelativeLocation();
			Loc.Z = VoidSphereBaseZ + 30.f * BobWave;
			VoidFloatingSphere->SetRelativeLocation(Loc);
		}

		// Pulse emissive on all materials for Void landmarks
		for (UMaterialInstanceDynamic* Mat : SpawnedMaterials)
		{
			if (Mat)
			{
				const float EmissivePulse = 0.5f + 0.5f * PulseWave;
				Mat->SetScalarParameterValue(TEXT("Emissive"), EmissivePulse);
			}
		}
	}
}

// =====================================================================
// ConfigureForBiome — Master dispatcher
// =====================================================================

void ATartariaLandmark::ConfigureForBiome()
{
	// Clear any previous geometry from a prior configuration
	ClearSpawnedMeshes();
	bVoidPulseActive = false;
	VoidFloatingSphere = nullptr;

	// Height scale factor (normalized to default 2000 UU)
	const float HeightScale = LandmarkHeight / 2000.f;

	switch (OwningBiome)
	{
	case ETartariaBiome::Clearinghouse:
		BeaconColor = FLinearColor(1.0f, 0.85f, 0.4f); // Gold
		BuildClearinghouseObelisk(HeightScale);
		break;

	case ETartariaBiome::Scriptorium:
		BeaconColor = FLinearColor(1.0f, 0.7f, 0.2f); // Amber
		BuildScriptoriumSpire(HeightScale);
		break;

	case ETartariaBiome::MonolithWard:
		BeaconColor = FLinearColor(0.4f, 0.6f, 1.0f); // Cold blue
		BuildMonolithWardSlab(HeightScale);
		break;

	case ETartariaBiome::ForgeDistrict:
		BeaconColor = FLinearColor(1.0f, 0.5f, 0.1f); // Orange
		BuildForgeDistrictChimney(HeightScale);
		break;

	case ETartariaBiome::VoidReach:
		BeaconColor = FLinearColor(0.6f, 0.2f, 0.9f); // Purple
		bVoidPulseActive = true;
		BuildVoidReachObelisk(HeightScale);
		break;
	}

	// Configure beacon light position, color, and intensity
	if (BeaconLight)
	{
		BeaconLight->SetRelativeLocation(FVector(0.f, 0.f, LandmarkHeight + 100.f * HeightScale));
		BeaconLight->SetLightColor(BeaconColor);
		BeaconLight->SetIntensity(BaseBeaconIntensity);
		BeaconLight->SetAttenuationRadius(BeaconAttenuationRadius);
	}

	// Hide the default placeholder mesh (replaced by biome-specific sub-meshes)
	if (LandmarkMesh)
	{
		LandmarkMesh->SetVisibility(false);
	}

	UE_LOG(LogTemp, Log, TEXT("TartariaLandmark: Configured for biome %d (height=%.0f)"),
		static_cast<int32>(OwningBiome), LandmarkHeight);
}

// =====================================================================
// Geometry Helper
// =====================================================================

UStaticMeshComponent* ATartariaLandmark::AddLandmarkPrimitive(
	const FName& Name, const TCHAR* ShapePath,
	const FVector& RelLoc, const FRotator& RelRot, const FVector& Scale,
	const FLinearColor& BaseColor, const FLinearColor& EmissiveColor,
	float EmissiveStrength)
{
	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this, Name);
	if (!Mesh) return nullptr;

	Mesh->SetupAttachment(RootComponent);
	Mesh->RegisterComponent();

	UStaticMesh* Shape = LoadObject<UStaticMesh>(nullptr, ShapePath);
	if (Shape)
	{
		Mesh->SetStaticMesh(Shape);
	}

	Mesh->SetRelativeLocation(RelLoc);
	Mesh->SetRelativeRotation(RelRot);
	Mesh->SetRelativeScale3D(Scale);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(true);

	// Apply dark silhouette material with subtle emissive edge
	UMaterialInstanceDynamic* DynMat = Mesh->CreateAndSetMaterialInstanceDynamic(0);
	if (DynMat)
	{
		DynMat->SetVectorParameterValue(TEXT("Color"), BaseColor);
		DynMat->SetVectorParameterValue(TEXT("EmissiveColor"), EmissiveColor);
		DynMat->SetScalarParameterValue(TEXT("Emissive"), EmissiveStrength);
		SpawnedMaterials.Add(DynMat);
	}

	SpawnedMeshes.Add(Mesh);
	return Mesh;
}

void ATartariaLandmark::ClearSpawnedMeshes()
{
	for (UStaticMeshComponent* Mesh : SpawnedMeshes)
	{
		if (Mesh)
		{
			Mesh->DestroyComponent();
		}
	}
	SpawnedMeshes.Empty();
	SpawnedMaterials.Empty();
}

// =====================================================================
// CLEARINGHOUSE: Tall obelisk with sphere cap — gold beacon
// =====================================================================

void ATartariaLandmark::BuildClearinghouseObelisk(float HeightScale)
{
	const TCHAR* Cube = TEXT("/Engine/BasicShapes/Cube");
	const TCHAR* Sphere = TEXT("/Engine/BasicShapes/Sphere");

	// Near-black stone with gold emissive edge
	const FLinearColor DarkStone(0.05f, 0.04f, 0.03f);
	const FLinearColor GoldEdge(1.0f, 0.85f, 0.4f);

	// Tall obelisk shaft — scaled cube (0.5, 0.5, 8.0)
	// Engine cube is 100x100x100 UU, so scale 0.5 = 50 UU wide, 8.0 = 800 UU tall
	const float ShaftHeight = 800.f * HeightScale;
	AddLandmarkPrimitive(
		FName(TEXT("Obelisk_Shaft")), Cube,
		FVector(0.f, 0.f, ShaftHeight * 0.5f),    // Center at half height
		FRotator::ZeroRotator,
		FVector(0.5f, 0.5f, 8.0f) * HeightScale,
		DarkStone, GoldEdge, 0.3f);

	// Sphere cap on top
	const float SphereRadius = 0.8f * HeightScale;
	AddLandmarkPrimitive(
		FName(TEXT("Obelisk_Cap")), Sphere,
		FVector(0.f, 0.f, ShaftHeight + 40.f * HeightScale),
		FRotator::ZeroRotator,
		FVector(SphereRadius, SphereRadius, SphereRadius),
		DarkStone, GoldEdge, 0.6f);

	// Small decorative base platform
	AddLandmarkPrimitive(
		FName(TEXT("Obelisk_Base")), Cube,
		FVector(0.f, 0.f, 25.f * HeightScale),
		FRotator::ZeroRotator,
		FVector(1.5f, 1.5f, 0.5f) * HeightScale,
		DarkStone, GoldEdge, 0.2f);
}

// =====================================================================
// SCRIPTORIUM: Crystal spire — 3 overlapping rotated cubes
// =====================================================================

void ATartariaLandmark::BuildScriptoriumSpire(float HeightScale)
{
	const TCHAR* Cube = TEXT("/Engine/BasicShapes/Cube");

	// Dark crystal with amber emissive
	const FLinearColor DarkCrystal(0.04f, 0.03f, 0.06f);
	const FLinearColor AmberEdge(1.0f, 0.7f, 0.2f);

	// Three overlapping cubes, each thinner and rotated 15 degrees more,
	// creating a twisted tower / crystal spire effect

	// Base segment — widest
	const float BaseHeight = 600.f * HeightScale;
	AddLandmarkPrimitive(
		FName(TEXT("Spire_Base")), Cube,
		FVector(0.f, 0.f, BaseHeight * 0.5f),
		FRotator(0.f, 0.f, 0.f),
		FVector(0.6f, 0.6f, 6.0f) * HeightScale,
		DarkCrystal, AmberEdge, 0.4f);

	// Middle segment — thinner, rotated 15 degrees
	const float MidHeight = 500.f * HeightScale;
	AddLandmarkPrimitive(
		FName(TEXT("Spire_Mid")), Cube,
		FVector(0.f, 0.f, BaseHeight + MidHeight * 0.5f),
		FRotator(0.f, 15.f, 0.f),
		FVector(0.45f, 0.45f, 5.0f) * HeightScale,
		DarkCrystal, AmberEdge, 0.5f);

	// Top segment — thinnest, rotated 30 degrees total
	const float TopHeight = 400.f * HeightScale;
	AddLandmarkPrimitive(
		FName(TEXT("Spire_Top")), Cube,
		FVector(0.f, 0.f, BaseHeight + MidHeight + TopHeight * 0.5f),
		FRotator(0.f, 30.f, 0.f),
		FVector(0.3f, 0.3f, 4.0f) * HeightScale,
		DarkCrystal, AmberEdge, 0.7f);

	// Small base pedestal
	AddLandmarkPrimitive(
		FName(TEXT("Spire_Pedestal")), Cube,
		FVector(0.f, 0.f, 20.f * HeightScale),
		FRotator::ZeroRotator,
		FVector(1.2f, 1.2f, 0.4f) * HeightScale,
		DarkCrystal, AmberEdge, 0.2f);
}

// =====================================================================
// MONOLITH_WARD: Massive flat monolith slab — cold blue beacon
// =====================================================================

void ATartariaLandmark::BuildMonolithWardSlab(float HeightScale)
{
	const TCHAR* Cube = TEXT("/Engine/BasicShapes/Cube");

	// Dark grey-blue monolith with cold blue emissive
	const FLinearColor DarkMonolith(0.06f, 0.06f, 0.08f);
	const FLinearColor ColdBlue(0.4f, 0.6f, 1.0f);

	// Main monolith slab — massive, flat, imposing: cube scaled (3.0, 0.3, 6.0)
	const float SlabHeight = 600.f * HeightScale;
	AddLandmarkPrimitive(
		FName(TEXT("Monolith_Slab")), Cube,
		FVector(0.f, 0.f, SlabHeight * 0.5f),
		FRotator::ZeroRotator,
		FVector(3.0f, 0.3f, 6.0f) * HeightScale,
		DarkMonolith, ColdBlue, 0.3f);

	// Secondary smaller slab slightly offset and angled — adds visual interest
	AddLandmarkPrimitive(
		FName(TEXT("Monolith_Secondary")), Cube,
		FVector(50.f * HeightScale, 30.f * HeightScale, SlabHeight * 0.35f),
		FRotator(0.f, 5.f, 2.f),
		FVector(1.5f, 0.2f, 4.0f) * HeightScale,
		DarkMonolith, ColdBlue, 0.4f);

	// Rough-hewn base — wider flat cube
	AddLandmarkPrimitive(
		FName(TEXT("Monolith_Base")), Cube,
		FVector(0.f, 0.f, 15.f * HeightScale),
		FRotator::ZeroRotator,
		FVector(4.0f, 1.0f, 0.3f) * HeightScale,
		DarkMonolith, ColdBlue, 0.15f);
}

// =====================================================================
// FORGE_DISTRICT: Forge chimney with smoke ball — orange beacon
// =====================================================================

void ATartariaLandmark::BuildForgeDistrictChimney(float HeightScale)
{
	const TCHAR* Cylinder = TEXT("/Engine/BasicShapes/Cylinder");
	const TCHAR* Sphere = TEXT("/Engine/BasicShapes/Sphere");

	// Dark iron with orange emissive glow
	const FLinearColor DarkIron(0.08f, 0.05f, 0.02f);
	const FLinearColor OrangeGlow(1.0f, 0.5f, 0.1f);

	// Chimney body — cylinder base (0.8, 0.8, 5.0)
	const float ChimneyHeight = 500.f * HeightScale;
	AddLandmarkPrimitive(
		FName(TEXT("Chimney_Body")), Cylinder,
		FVector(0.f, 0.f, ChimneyHeight * 0.5f),
		FRotator::ZeroRotator,
		FVector(0.8f, 0.8f, 5.0f) * HeightScale,
		DarkIron, OrangeGlow, 0.4f);

	// Chimney lip — slightly wider cylinder ring at the top
	AddLandmarkPrimitive(
		FName(TEXT("Chimney_Lip")), Cylinder,
		FVector(0.f, 0.f, ChimneyHeight + 5.f * HeightScale),
		FRotator::ZeroRotator,
		FVector(1.0f, 1.0f, 0.3f) * HeightScale,
		DarkIron, OrangeGlow, 0.6f);

	// Smoke ball — small sphere floating 200 units above chimney top
	const float SmokeOffset = 200.f * HeightScale;
	AddLandmarkPrimitive(
		FName(TEXT("Chimney_SmokeBall")), Sphere,
		FVector(0.f, 0.f, ChimneyHeight + SmokeOffset),
		FRotator::ZeroRotator,
		FVector(0.6f, 0.6f, 0.6f) * HeightScale,
		FLinearColor(0.15f, 0.1f, 0.05f), // Slightly lighter — smoky
		OrangeGlow, 0.8f);

	// Industrial base — wider cylinder
	AddLandmarkPrimitive(
		FName(TEXT("Chimney_Base")), Cylinder,
		FVector(0.f, 0.f, 25.f * HeightScale),
		FRotator::ZeroRotator,
		FVector(1.4f, 1.4f, 0.5f) * HeightScale,
		DarkIron, OrangeGlow, 0.2f);
}

// =====================================================================
// VOID_REACH: Inverted cone + floating sphere — purple pulsing beacon
// =====================================================================

void ATartariaLandmark::BuildVoidReachObelisk(float HeightScale)
{
	const TCHAR* Cylinder = TEXT("/Engine/BasicShapes/Cylinder");
	const TCHAR* Sphere = TEXT("/Engine/BasicShapes/Sphere");
	const TCHAR* Cube = TEXT("/Engine/BasicShapes/Cube");

	// Near-black void material with purple emissive
	const FLinearColor VoidBlack(0.02f, 0.01f, 0.04f);
	const FLinearColor PurpleEdge(0.6f, 0.2f, 0.9f);

	// Inverted cone effect — cylinder tapered via non-uniform scale
	// The cylinder narrows at the bottom and widens at top (inverted taper)
	// Achieved by rotating 180 degrees and using a tapered-look assembly
	const float ConeHeight = 500.f * HeightScale;

	// Lower narrow section
	AddLandmarkPrimitive(
		FName(TEXT("Void_LowerTaper")), Cube,
		FVector(0.f, 0.f, ConeHeight * 0.25f),
		FRotator::ZeroRotator,
		FVector(0.3f, 0.3f, 2.5f) * HeightScale,
		VoidBlack, PurpleEdge, 0.5f);

	// Upper wider section (inverted cone appearance)
	AddLandmarkPrimitive(
		FName(TEXT("Void_UpperFlare")), Cube,
		FVector(0.f, 0.f, ConeHeight * 0.7f),
		FRotator(0.f, 45.f, 0.f),
		FVector(0.8f, 0.8f, 3.0f) * HeightScale,
		VoidBlack, PurpleEdge, 0.6f);

	// Central cylinder spine
	AddLandmarkPrimitive(
		FName(TEXT("Void_Spine")), Cylinder,
		FVector(0.f, 0.f, ConeHeight * 0.5f),
		FRotator::ZeroRotator,
		FVector(0.2f, 0.2f, 5.0f) * HeightScale,
		VoidBlack, PurpleEdge, 0.4f);

	// Floating sphere — offset to one side and above, pulsing via Tick()
	const float SphereZBase = ConeHeight + 250.f * HeightScale;
	VoidFloatingSphere = AddLandmarkPrimitive(
		FName(TEXT("Void_FloatingSphere")), Sphere,
		FVector(50.f * HeightScale, 30.f * HeightScale, SphereZBase),
		FRotator::ZeroRotator,
		FVector(0.7f, 0.7f, 0.7f) * HeightScale,
		VoidBlack, PurpleEdge, 0.8f);

	// Cache base Z for bobbing animation
	VoidSphereBaseZ = SphereZBase;

	// Fractured base ring — broken/unstable appearance
	AddLandmarkPrimitive(
		FName(TEXT("Void_BaseRing")), Cylinder,
		FVector(0.f, 0.f, 15.f * HeightScale),
		FRotator(0.f, 0.f, 3.f), // Slight tilt — unstable
		FVector(1.2f, 1.2f, 0.3f) * HeightScale,
		VoidBlack, PurpleEdge, 0.3f);
}
