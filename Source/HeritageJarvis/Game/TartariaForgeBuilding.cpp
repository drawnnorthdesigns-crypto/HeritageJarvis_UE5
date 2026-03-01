#include "TartariaForgeBuilding.h"
#include "TartariaGameMode.h"
#include "TartariaTerminal.h"
#include "TartariaRewardVFX.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "ProceduralMeshComponent.h"
#include "Core/HJMeshLoader.h"
#include "Core/HJGameInstance.h"
#include "Core/HJApiClient.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
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

	// ── Completion Burst Light: golden upward flash on pipeline completion ──
	CompletionBurstLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("CompletionBurstLight"));
	CompletionBurstLight->SetupAttachment(PedestalMesh);
	CompletionBurstLight->SetRelativeLocation(FVector(0.f, 0.f, 200.f));
	CompletionBurstLight->SetIntensity(0.f); // Off until burst
	CompletionBurstLight->SetLightColor(FLinearColor(1.0f, 0.85f, 0.3f)); // Bright gold
	CompletionBurstLight->SetAttenuationRadius(1200.f);

	// ── Forge Queue Counter: Progress-driven chimney light ──
	ChimneyLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ChimneyLight"));
	ChimneyLight->SetupAttachment(RootComponent);
	ChimneyLight->SetRelativeLocation(FVector(120.f, 0.f, 550.f));
	ChimneyLight->SetIntensity(500.f); // Dim idle default
	ChimneyLight->SetLightColor(FLinearColor(0.4f, 0.4f, 0.4f)); // Grey when idle
	ChimneyLight->SetAttenuationRadius(700.f);

	// ── Forge Queue Counter: Floating progress text ──
	ProgressText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ProgressText"));
	ProgressText->SetupAttachment(RootComponent);
	ProgressText->SetRelativeLocation(FVector(0.f, 0.f, 650.f));
	ProgressText->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	ProgressText->SetText(FText::FromString(TEXT("Forge Idle")));
	ProgressText->SetTextRenderColor(FColor(136, 136, 136)); // Grey
	ProgressText->SetWorldSize(24.f);
	ProgressText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
}

// -------------------------------------------------------
// BeginPlay
// -------------------------------------------------------

void ATartariaForgeBuilding::BeginPlay()
{
	Super::BeginPlay();

	InitProxyPool();
	InitEmberPool();
}

// -------------------------------------------------------
// Proxy Primitive Object Pool
// -------------------------------------------------------

void ATartariaForgeBuilding::InitProxyPool()
{
	ProxyPool.Reserve(PROXY_POOL_SIZE);
	ProxyPoolInUse.Init(false, PROXY_POOL_SIZE);

	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Engine/BasicShapes/Cube.Cube"));

	for (int32 i = 0; i < PROXY_POOL_SIZE; ++i)
	{
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this,
			*FString::Printf(TEXT("ProxyPool_%d"), i));
		Comp->SetupAttachment(RootComponent);
		Comp->RegisterComponent();
		Comp->SetStaticMesh(CubeMesh);  // Default shape, overridden on acquire
		Comp->SetVisibility(false);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// Translucent golden material
		UMaterialInstanceDynamic* Mat = Comp->CreateDynamicMaterialInstance(0);
		if (Mat)
		{
			Mat->SetVectorParameterValue(TEXT("BaseColor"),
				FLinearColor(0.78f, 0.63f, 0.3f, 0.4f));
		}

		ProxyPool.Add(Comp);
	}
}

UStaticMeshComponent* ATartariaForgeBuilding::AcquireProxyPrimitive()
{
	for (int32 i = 0; i < PROXY_POOL_SIZE; ++i)
	{
		if (!ProxyPoolInUse[i])
		{
			ProxyPoolInUse[i] = true;
			++ActiveProxyCount;

			UStaticMeshComponent* Comp = ProxyPool[i];
			if (Comp)
			{
				Comp->SetVisibility(true);
			}
			return Comp;
		}
	}

	// Pool exhausted — log warning once
	if (!bPoolExhaustedWarned)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Forge '%s': Proxy pool exhausted (%d/%d). Skipping additional proxy primitives."),
			*BuildingName, ActiveProxyCount, PROXY_POOL_SIZE);
		bPoolExhaustedWarned = true;
	}
	return nullptr;
}

void ATartariaForgeBuilding::ReleaseProxyPrimitive(UStaticMeshComponent* Comp)
{
	if (!Comp) return;

	int32 Index = ProxyPool.Find(Comp);
	if (Index == INDEX_NONE) return;

	Comp->SetVisibility(false);
	Comp->SetRelativeTransform(FTransform::Identity);
	Comp->SetRelativeScale3D(FVector::OneVector);

	ProxyPoolInUse[Index] = false;
	--ActiveProxyCount;
}

void ATartariaForgeBuilding::ReleaseAllProxies()
{
	for (int32 i = 0; i < ProxyPool.Num(); ++i)
	{
		if (ProxyPoolInUse[i] && ProxyPool[i])
		{
			ProxyPool[i]->SetVisibility(false);
			ProxyPool[i]->SetRelativeTransform(FTransform::Identity);
			ProxyPool[i]->SetRelativeScale3D(FVector::OneVector);
			ProxyPoolInUse[i] = false;
		}
	}
	ActiveProxyCount = 0;
	bPoolExhaustedWarned = false;
}

// -------------------------------------------------------
// Ember Particle Pool
// -------------------------------------------------------

void ATartariaForgeBuilding::InitEmberPool()
{
	EmberPool.Reserve(MAX_EMBERS);
	EmberMaterials.Reserve(MAX_EMBERS);
	EmberLifetimes.Init(0.f, MAX_EMBERS);
	EmberMaxLifetimes.Init(1.f, MAX_EMBERS);
	EmberVelocities.Init(FVector::ZeroVector, MAX_EMBERS);
	EmberScales.Init(FVector(0.03f), MAX_EMBERS);

	UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	for (int32 i = 0; i < MAX_EMBERS; ++i)
	{
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this,
			*FString::Printf(TEXT("EmberPool_%d"), i));
		Comp->SetupAttachment(RootComponent);
		Comp->RegisterComponent();
		if (SphereMesh)
		{
			Comp->SetStaticMesh(SphereMesh);
		}
		Comp->SetVisibility(false);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetCastShadow(false);
		Comp->SetRelativeScale3D(FVector(0.03f));

		// Create bright emissive orange material
		UMaterialInstanceDynamic* Mat = Comp->CreateAndSetMaterialInstanceDynamic(0);
		if (Mat)
		{
			// Bright orange-red ember color with strong emissive
			FLinearColor EmberColor(1.0f, 0.4f, 0.05f);
			Mat->SetVectorParameterValue(TEXT("Color"), EmberColor);
			Mat->SetVectorParameterValue(TEXT("EmissiveColor"), EmberColor * 5.0f);
			Mat->SetScalarParameterValue(TEXT("Emissive"), 5.0f);
		}

		EmberPool.Add(Comp);
		EmberMaterials.Add(Mat);
	}
}

void ATartariaForgeBuilding::SpawnEmber()
{
	// Find an inactive ember slot
	int32 SlotIndex = INDEX_NONE;
	for (int32 i = 0; i < MAX_EMBERS; ++i)
	{
		if (EmberLifetimes[i] <= 0.f)
		{
			SlotIndex = i;
			break;
		}
	}

	if (SlotIndex == INDEX_NONE) return; // All slots active

	UStaticMeshComponent* Comp = EmberPool[SlotIndex];
	if (!Comp) return;

	// Randomize lifetime: 1.0 - 3.0 seconds
	float Lifetime = FMath::FRandRange(1.0f, 3.0f);
	EmberLifetimes[SlotIndex] = Lifetime;
	EmberMaxLifetimes[SlotIndex] = Lifetime;

	// Randomize scale: 2-5 cm (0.02 - 0.05 in UE units for a 100cm unit sphere)
	float ScaleFactor = FMath::FRandRange(0.02f, 0.05f);
	FVector Scale(ScaleFactor);
	EmberScales[SlotIndex] = Scale;
	Comp->SetRelativeScale3D(Scale);

	// Spawn position: near chimney top with some random spread
	// Chimney is at roughly (120, 0, 550) based on ChimneyLight position
	FVector SpawnPos(
		120.f + FMath::FRandRange(-30.f, 30.f),
		FMath::FRandRange(-30.f, 30.f),
		550.f + FMath::FRandRange(-20.f, 20.f)
	);
	Comp->SetRelativeLocation(SpawnPos);

	// Velocity: upward 50-150 cm/s, random X/Y drift +/-20 cm/s
	FVector Velocity(
		FMath::FRandRange(-20.f, 20.f),
		FMath::FRandRange(-20.f, 20.f),
		FMath::FRandRange(50.f, 150.f)
	);
	EmberVelocities[SlotIndex] = Velocity;

	// Reset material to full brightness
	if (EmberMaterials[SlotIndex])
	{
		FLinearColor EmberColor(1.0f, 0.4f, 0.05f);
		EmberMaterials[SlotIndex]->SetVectorParameterValue(TEXT("EmissiveColor"), EmberColor * 5.0f);
		EmberMaterials[SlotIndex]->SetScalarParameterValue(TEXT("Emissive"), 5.0f);
	}

	Comp->SetVisibility(true);
}

void ATartariaForgeBuilding::UpdateEmbers(float DeltaTime)
{
	for (int32 i = 0; i < MAX_EMBERS; ++i)
	{
		if (EmberLifetimes[i] <= 0.f) continue;

		EmberLifetimes[i] -= DeltaTime;

		UStaticMeshComponent* Comp = EmberPool[i];
		if (!Comp) continue;

		if (EmberLifetimes[i] <= 0.f)
		{
			// Ember expired — hide and recycle
			EmberLifetimes[i] = 0.f;
			Comp->SetVisibility(false);
			continue;
		}

		// Move ember: apply velocity
		FVector CurrentLoc = Comp->GetRelativeLocation();
		CurrentLoc += EmberVelocities[i] * DeltaTime;

		// Add slight swirl/turbulence
		float LifeRatio = EmberLifetimes[i] / EmberMaxLifetimes[i];
		float Swirl = FMath::Sin(EmberLifetimes[i] * 8.f) * 5.f * DeltaTime;
		CurrentLoc.X += Swirl;
		CurrentLoc.Y += FMath::Cos(EmberLifetimes[i] * 6.f) * 3.f * DeltaTime;

		Comp->SetRelativeLocation(CurrentLoc);

		// Fade: shrink scale and reduce emissive as ember dies
		float FadeAlpha = FMath::Clamp(LifeRatio, 0.f, 1.f);

		// Scale shrinks in the last 40% of life
		float ScaleMult = (LifeRatio < 0.4f) ? (LifeRatio / 0.4f) : 1.0f;
		Comp->SetRelativeScale3D(EmberScales[i] * ScaleMult);

		// Emissive fades: bright orange -> dim red -> dark
		if (EmberMaterials[i])
		{
			// Color shifts from orange to red as it cools
			FLinearColor FadedColor = FMath::Lerp(
				FLinearColor(0.8f, 0.1f, 0.0f),   // Dying: dim red
				FLinearColor(1.0f, 0.4f, 0.05f),   // Fresh: bright orange
				FadeAlpha
			);
			float EmissiveStrength = 5.0f * FadeAlpha * FadeAlpha; // Quadratic falloff
			EmberMaterials[i]->SetVectorParameterValue(TEXT("EmissiveColor"), FadedColor * EmissiveStrength);
			EmberMaterials[i]->SetScalarParameterValue(TEXT("Emissive"), EmissiveStrength);
		}
	}
}

void ATartariaForgeBuilding::SetEmberSpawnRate(float Rate)
{
	EmberSpawnRate = FMath::Clamp(Rate, 0.f, 50.f);
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

	// ── Materialization Burst Animation ──
	if (bMaterializationBurstActive)
	{
		BurstElapsed += DeltaTime;
		float T = BurstElapsed / BurstDuration;

		// Phase 1 (0-0.3s): Pedestal scale up 1.0 -> 1.3
		if (BurstElapsed < 0.3f)
		{
			float ScaleT = BurstElapsed / 0.3f;
			float ScaleFactor = 1.0f + 0.3f * FMath::InterpEaseOut(0.f, 1.f, ScaleT, 2.0f);
			if (PedestalMesh)
			{
				PedestalMesh->SetRelativeScale3D(OriginalPedestalScale * ScaleFactor);
			}
		}
		// Phase 2 (0.3-0.8s): Pedestal scale back down 1.3 -> 1.0
		else if (BurstElapsed < 0.8f)
		{
			float ScaleT = (BurstElapsed - 0.3f) / 0.5f;
			float ScaleFactor = 1.3f - 0.3f * FMath::InterpEaseIn(0.f, 1.f, ScaleT, 2.0f);
			if (PedestalMesh)
			{
				PedestalMesh->SetRelativeScale3D(OriginalPedestalScale * ScaleFactor);
			}
		}

		// Phase 1 (0-0.5s): Building light stays bright gold
		if (BurstElapsed >= 0.5f && BurstElapsed < BurstDuration)
		{
			// Fade building light back to normal over remaining time
			float FadeT = (BurstElapsed - 0.5f) / (BurstDuration - 0.5f);
			float FadeIntensity = FMath::Lerp(BaseForgeIntensity * 5.0f,
				bForgeActive ? BaseForgeIntensity : IdleIntensity, FadeT);
			if (BuildingLight)
			{
				BuildingLight->SetIntensity(FadeIntensity);
				// Transition color back from gold to forge color
				FLinearColor LerpColor = FMath::Lerp(
					FLinearColor(1.0f, 0.9f, 0.4f), BaseForgeColor, FadeT);
				BuildingLight->SetLightColor(LerpColor);
			}
		}

		// Fade completion burst light over full duration
		if (CompletionBurstLight)
		{
			float BurstFade = FMath::Clamp(1.0f - T, 0.f, 1.f);
			CompletionBurstLight->SetIntensity(25000.f * BurstFade * BurstFade);
		}

		// Fade window emissive back to normal
		float WindowEmissive = FMath::Lerp(15.0f, bForgeActive ? 5.0f : 2.0f, FMath::Clamp(T, 0.f, 1.f));
		for (int32 i = 0; i < WindowMaterials.Num(); ++i)
		{
			if (WindowMaterials[i])
			{
				WindowMaterials[i]->SetScalarParameterValue(TEXT("Emissive"), WindowEmissive);
			}
		}

		// End burst
		if (BurstElapsed >= BurstDuration)
		{
			bMaterializationBurstActive = false;
			if (PedestalMesh)
			{
				PedestalMesh->SetRelativeScale3D(OriginalPedestalScale);
			}
			if (CompletionBurstLight)
			{
				CompletionBurstLight->SetIntensity(0.f);
			}
			// Restore window colors
			FLinearColor ForgeGlow(1.0f, 0.6f, 0.15f);
			for (int32 i = 0; i < WindowMaterials.Num(); ++i)
			{
				if (WindowMaterials[i])
				{
					WindowMaterials[i]->SetVectorParameterValue(TEXT("Color"), ForgeGlow);
				}
			}
		}
	}

	// ── Forge Queue Counter: Poll every 5 seconds (per-forge filtered) ──
	QueuePollTimer += DeltaTime;
	if (QueuePollTimer >= 5.0f)
	{
		QueuePollTimer = 0.0f;
		// Use PollForgeStatus() when this forge has a BuildingId so the
		// backend filters results to only this forge's assigned job.
		// Fall back to PollQueueStatus() for forges without an ID (e.g.
		// placed in editor without a BuildingId set).
		if (!BuildingId.IsEmpty())
		{
			PollForgeStatus();
		}
		else
		{
			PollQueueStatus();
		}
	}

	// ── Live Proxy: Poll every 2 seconds while forge has active job ──
	if (bQueueHasActiveJob || bProxyActive)
	{
		ProxyPollTimer += DeltaTime;
		if (ProxyPollTimer >= 2.0f)
		{
			ProxyPollTimer = 0.0f;
			PollLiveProxy();
		}
	}

	// ── Live Proxy: Detect job completion (active -> inactive transition) ──
	if (bPreviousQueueHasActiveJob && !bQueueHasActiveJob && bProxyActive)
	{
		// Job just completed — solidify the proxy geometry
		SolidifyProxy();

		// Schedule proxy clear after 3 seconds to show solidified result
		ProxyClearCountdown = 3.0f;
	}
	bPreviousQueueHasActiveJob = bQueueHasActiveJob;

	// ── Live Proxy: Countdown to clear after solidification ──
	if (ProxyClearCountdown > 0.0f)
	{
		ProxyClearCountdown -= DeltaTime;
		if (ProxyClearCountdown <= 0.0f)
		{
			ProxyClearCountdown = -1.0f;
			ClearProxyGeometry();
		}
	}

	// ── Live Proxy: Scale-up animations for newly spawned primitives ──
	for (int32 i = ProxyScaleAnims.Num() - 1; i >= 0; --i)
	{
		FProxyScaleAnim& Anim = ProxyScaleAnims[i];
		Anim.Elapsed += DeltaTime;
		float Alpha = FMath::Clamp(Anim.Elapsed / Anim.Duration, 0.0f, 1.0f);
		// Ease-out curve for smooth appearance
		float EasedAlpha = FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 2.0f);

		if (Anim.Index >= 0 && Anim.Index < ProxyPrimitives.Num() && ProxyPrimitives[Anim.Index])
		{
			FVector CurrentScale = Anim.TargetScale * EasedAlpha;
			ProxyPrimitives[Anim.Index]->SetRelativeScale3D(CurrentScale);
		}

		if (Alpha >= 1.0f)
		{
			ProxyScaleAnims.RemoveAt(i);
		}
	}

	// ── Live Proxy: Solidification animation ──
	if (bSolidifyActive)
	{
		SolidifyElapsed += DeltaTime;
		float T = FMath::Clamp(SolidifyElapsed / SolidifyDuration, 0.0f, 1.0f);

		// Lerp opacity from 0.4 to 1.0 across all proxy primitives
		float CurrentOpacity = FMath::Lerp(0.4f, 1.0f, T);
		// Emissive flash: peak at T=0.3, then fade
		float EmissiveMultiplier = (T < 0.3f)
			? FMath::Lerp(1.0f, 4.0f, T / 0.3f)
			: FMath::Lerp(4.0f, 0.5f, (T - 0.3f) / 0.7f);

		for (UStaticMeshComponent* Comp : ProxyPrimitives)
		{
			if (!Comp) continue;
			UMaterialInstanceDynamic* DynMat = Cast<UMaterialInstanceDynamic>(Comp->GetMaterial(0));
			if (DynMat)
			{
				DynMat->SetScalarParameterValue(TEXT("Opacity"), CurrentOpacity);
				FLinearColor EmissiveColor = FLinearColor(0.3f, 0.2f, 0.05f) * EmissiveMultiplier;
				DynMat->SetVectorParameterValue(TEXT("EmissiveColor"), EmissiveColor);
			}
		}

		if (T >= 1.0f)
		{
			bSolidifyActive = false;
		}
	}

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

	// ── Billboard: ProgressText faces player camera ──
	if (ProgressText)
	{
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (PC)
		{
			FVector CamLoc;
			FRotator CamRot;
			PC->GetPlayerViewPoint(CamLoc, CamRot);
			FRotator LookAt = UKismetMathLibrary::FindLookAtRotation(
				ProgressText->GetComponentLocation(), CamLoc);
			ProgressText->SetWorldRotation(LookAt);
		}
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

// -------------------------------------------------------
// Materialization Burst VFX
// -------------------------------------------------------

void ATartariaForgeBuilding::PlayMaterializationBurst()
{
	if (BuildingType != ETartariaBuildingType::Forge) return;

	UE_LOG(LogTemp, Log, TEXT("Forge '%s': Playing materialization burst VFX"), *BuildingName);

	bMaterializationBurstActive = true;
	BurstElapsed = 0.0f;

	// Save original pedestal scale for restoration
	if (PedestalMesh)
	{
		OriginalPedestalScale = PedestalMesh->GetRelativeScale3D();
	}

	// Immediate: flash building light to bright gold
	if (BuildingLight)
	{
		BuildingLight->SetIntensity(BaseForgeIntensity * 5.0f);
		BuildingLight->SetLightColor(FLinearColor(1.0f, 0.9f, 0.4f)); // Bright gold
	}

	// Activate completion burst light (golden upward sparks)
	if (CompletionBurstLight)
	{
		CompletionBurstLight->SetIntensity(25000.f);
	}

	// Flash all window materials to maximum emissive
	for (int32 i = 0; i < WindowMaterials.Num(); ++i)
	{
		if (WindowMaterials[i])
		{
			WindowMaterials[i]->SetScalarParameterValue(TEXT("Emissive"), 15.0f);
			WindowMaterials[i]->SetVectorParameterValue(TEXT("Color"),
				FLinearColor(1.0f, 0.85f, 0.3f)); // Gold
		}
	}

	// Chimney flicker burst
	if (ChimneyFlickerLight)
	{
		ChimneyFlickerLight->SetIntensity(15000.f);
	}

	// Spawn reward VFX at building location for additional particle flair
	ATartariaRewardVFX::SpawnRewardVFX(
		this, GetActorLocation() + FVector(0.f, 0.f, 300.f),
		ERewardVFXType::Materialization, 2.5f);
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

// -------------------------------------------------------
// Forge Queue Counter — Poll + Visuals
// -------------------------------------------------------

void ATartariaForgeBuilding::PollQueueStatus()
{
	UHJGameInstance* GI = UHJGameInstance::Get(this);
	if (!GI || !GI->ApiClient) return;

	TWeakObjectPtr<ATartariaForgeBuilding> WeakThis(this);

	FOnApiResponse CB;
	CB.BindLambda([WeakThis](bool bOk, const FString& RespBody)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, bOk, RespBody]()
		{
			ATartariaForgeBuilding* Self = WeakThis.Get();
			if (!Self) return;

			if (!bOk) return;

			TSharedPtr<FJsonObject> Json;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RespBody);
			if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
				return;

			Self->QueueCompletedToday = Json->GetIntegerField(TEXT("completed_today"));
			Self->QueueTotalJobs = Json->GetIntegerField(TEXT("total_jobs"));

			const TSharedPtr<FJsonObject>* ActiveJobObj = nullptr;
			if (Json->TryGetObjectField(TEXT("active_job"), ActiveJobObj) && ActiveJobObj && ActiveJobObj->IsValid())
			{
				Self->bQueueHasActiveJob = true;
				Self->QueueProgressPct = (*ActiveJobObj)->GetIntegerField(TEXT("progress_pct"));
				Self->QueueActiveStageName = (*ActiveJobObj)->GetStringField(TEXT("stage"));
			}
			else
			{
				Self->bQueueHasActiveJob = false;
				Self->QueueProgressPct = 0;
				Self->QueueActiveStageName = TEXT("");
			}

			Self->ApplyQueueVisuals();
		});
	});

	GI->ApiClient->Get(TEXT("/api/engineering/queue-status"), CB);
}

void ATartariaForgeBuilding::PollForgeStatus()
{
	// Build the per-forge URL: /api/engineering/queue-status?forge_id=<BuildingId>
	// The backend will filter active_job to only the job assigned to this forge.
	if (BuildingId.IsEmpty()) return;

	UHJGameInstance* GI = UHJGameInstance::Get(this);
	if (!GI || !GI->ApiClient) return;

	FString Url = FString::Printf(
		TEXT("/api/engineering/queue-status?forge_id=%s"), *BuildingId);

	TWeakObjectPtr<ATartariaForgeBuilding> WeakThis(this);

	FOnApiResponse CB;
	CB.BindLambda([WeakThis](bool bOk, const FString& RespBody)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, bOk, RespBody]()
		{
			ATartariaForgeBuilding* Self = WeakThis.Get();
			if (!Self) return;

			if (!bOk) return;

			TSharedPtr<FJsonObject> Json;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RespBody);
			if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
				return;

			// Update global queue counters (same as the unfiltered poll)
			Self->QueueCompletedToday = Json->GetIntegerField(TEXT("completed_today"));
			Self->QueueTotalJobs      = Json->GetIntegerField(TEXT("total_jobs"));

			// active_job is now filtered to THIS forge's assigned job only.
			// If null → forge has no active job → show idle state.
			const TSharedPtr<FJsonObject>* ActiveJobObj = nullptr;
			if (Json->TryGetObjectField(TEXT("active_job"), ActiveJobObj)
				&& ActiveJobObj && ActiveJobObj->IsValid())
			{
				Self->bQueueHasActiveJob   = true;
				Self->QueueProgressPct     = (*ActiveJobObj)->GetIntegerField(TEXT("progress_pct"));
				Self->QueueActiveStageName = (*ActiveJobObj)->GetStringField(TEXT("stage"));

				UE_LOG(LogTemp, Verbose,
					TEXT("Forge '%s' [%s]: assigned job active — stage=%s pct=%d"),
					*Self->BuildingName, *Self->BuildingId,
					*Self->QueueActiveStageName, Self->QueueProgressPct);
			}
			else
			{
				// No job assigned or assigned job is not running
				Self->bQueueHasActiveJob   = false;
				Self->QueueProgressPct     = 0;
				Self->QueueActiveStageName = TEXT("");

				// Log once (at Verbose) when the forge just went idle
				UE_LOG(LogTemp, Verbose,
					TEXT("Forge '%s' [%s]: no assigned job active"),
					*Self->BuildingName, *Self->BuildingId);
			}

			Self->ApplyQueueVisuals();
		});
	});

	GI->ApiClient->Get(Url, CB);
}

void ATartariaForgeBuilding::ApplyQueueVisuals()
{
	// ── ChimneyLight: intensity proportional to progress_pct ──
	if (ChimneyLight)
	{
		if (bQueueHasActiveJob)
		{
			// Map 0-100 progress to 500-5000 intensity
			float Intensity = 500.f + (QueueProgressPct / 100.f) * 4500.f;
			ChimneyLight->SetIntensity(Intensity);
			ChimneyLight->SetLightColor(FLinearColor(1.0f, 0.6f, 0.1f)); // Warm orange
		}
		else
		{
			ChimneyLight->SetIntensity(500.f);
			ChimneyLight->SetLightColor(FLinearColor(0.4f, 0.4f, 0.4f)); // Dim grey
		}
	}

	// ── ProgressText: floating label ──
	if (ProgressText)
	{
		if (bQueueHasActiveJob)
		{
			FString Label = FString::Printf(TEXT("%d/%d -- %s %d%%"),
				QueueCompletedToday, QueueTotalJobs,
				*QueueActiveStageName, QueueProgressPct);
			ProgressText->SetText(FText::FromString(Label));
			ProgressText->SetTextRenderColor(FColor(201, 168, 76)); // Gold
		}
		else
		{
			ProgressText->SetText(FText::FromString(TEXT("Forge Idle")));
			ProgressText->SetTextRenderColor(FColor(136, 136, 136)); // Grey
		}
	}
}

// -------------------------------------------------------
// Live-Thinking Proxy Geometry
// -------------------------------------------------------

void ATartariaForgeBuilding::PollLiveProxy()
{
	if (BuildingType != ETartariaBuildingType::Forge) return;

	UHJGameInstance* GI = UHJGameInstance::Get(this);
	if (!GI || !GI->ApiClient) return;

	TWeakObjectPtr<ATartariaForgeBuilding> WeakThis(this);

	FOnApiResponse CB;
	CB.BindLambda([WeakThis](bool bOk, const FString& RespBody)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, bOk, RespBody]()
		{
			ATartariaForgeBuilding* Self = WeakThis.Get();
			if (!Self) return;

			if (!bOk) return;

			TSharedPtr<FJsonObject> Json;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RespBody);
			if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
				return;

			FString Phase;
			Json->TryGetStringField(TEXT("phase"), Phase);

			// Only process during active generation phases
			if (Phase == TEXT("idle") || Phase == TEXT("error"))
			{
				if (Self->bProxyActive && Phase == TEXT("idle"))
				{
					// Generation ended — the Tick job-completion detector will handle solidify
				}
				return;
			}

			// Parse the primitives field — it's a JSON string containing an array
			FString PrimitivesStr;
			Json->TryGetStringField(TEXT("primitives"), PrimitivesStr);

			if (PrimitivesStr.IsEmpty() || PrimitivesStr == TEXT("[]"))
				return;

			// Parse the inner JSON array of primitives
			TArray<TSharedPtr<FJsonValue>> PrimArray;
			TSharedRef<TJsonReader<>> PrimReader = TJsonReaderFactory<>::Create(PrimitivesStr);
			if (!FJsonSerializer::Deserialize(PrimReader, PrimArray))
				return;

			int32 IncomingCount = PrimArray.Num();
			int32 CurrentCount = Self->ProxyPrimitives.Num();

			if (IncomingCount <= 0) return;

			Self->bProxyActive = true;

			// Create the proxy material if we haven't yet
			if (!Self->ProxyMaterial)
			{
				// Load the engine default material as base
				UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(
					nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial"));
				if (BaseMat)
				{
					Self->ProxyMaterial = UMaterialInstanceDynamic::Create(BaseMat, Self);
					if (Self->ProxyMaterial)
					{
						// Golden translucent appearance
						Self->ProxyMaterial->SetVectorParameterValue(
							TEXT("Color"), FLinearColor(0.9f, 0.75f, 0.2f));
						Self->ProxyMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.4f);
						Self->ProxyMaterial->SetVectorParameterValue(
							TEXT("EmissiveColor"), FLinearColor(0.3f, 0.2f, 0.05f));
					}
				}
			}

			// Only spawn NEW primitives (those beyond our current count)
			for (int32 i = CurrentCount; i < IncomingCount; ++i)
			{
				const TSharedPtr<FJsonObject>* PrimObj = nullptr;
				if (!PrimArray[i]->TryGetObject(PrimObj) || !PrimObj || !(*PrimObj).IsValid())
					continue;

				FString Geometry;
				(*PrimObj)->TryGetStringField(TEXT("geometry"), Geometry);

				// Get params sub-object
				const TSharedPtr<FJsonObject>* ParamsObj = nullptr;
				if (!(*PrimObj)->TryGetObjectField(TEXT("params"), ParamsObj) || !ParamsObj)
					continue;

				// Determine type and scale from params
				FString PrimType;
				FVector PrimScale = FVector(1.0f);

				if (Geometry == TEXT("box"))
				{
					PrimType = TEXT("box");
					float Width = (*ParamsObj)->GetNumberField(TEXT("width"));
					float Height = (*ParamsObj)->GetNumberField(TEXT("height"));
					float Depth = (*ParamsObj)->GetNumberField(TEXT("depth"));
					// Primitives are in mm — convert to cm and scale relative to unit cube (100 UU)
					PrimScale = FVector(
						Width * 0.1f / 100.0f,
						Depth * 0.1f / 100.0f,
						Height * 0.1f / 100.0f
					);
				}
				else if (Geometry == TEXT("cylinder") || Geometry == TEXT("cylinder_subtract"))
				{
					PrimType = TEXT("cylinder");
					float Radius = (*ParamsObj)->GetNumberField(TEXT("radius"));
					float CylHeight = 10.0f;
					(*ParamsObj)->TryGetNumberField(TEXT("height"), CylHeight);
					if (!(*ParamsObj)->HasField(TEXT("height")))
					{
						// cylinder_subtract uses "depth" instead of "height"
						(*ParamsObj)->TryGetNumberField(TEXT("depth"), CylHeight);
					}
					float DiameterCm = Radius * 2.0f * 0.1f;
					float HeightCm = CylHeight * 0.1f;
					PrimScale = FVector(
						DiameterCm / 100.0f,
						DiameterCm / 100.0f,
						HeightCm / 100.0f
					);
				}
				else if (Geometry == TEXT("sphere"))
				{
					PrimType = TEXT("sphere");
					float Radius = (*ParamsObj)->GetNumberField(TEXT("radius"));
					float DiameterCm = Radius * 2.0f * 0.1f;
					PrimScale = FVector(DiameterCm / 100.0f);
				}
				else
				{
					// Unknown geometry type — skip
					continue;
				}

				// Position: stack vertically above pedestal, offset per primitive index
				// Each primitive offsets upward so they don't all overlap
				float VerticalOffset = (float)i * 15.0f; // 15 cm per primitive
				FVector Location = FVector(0.0f, 0.0f, VerticalOffset);

				Self->AddProxyPrimitive(PrimType, Location, PrimScale, FRotator::ZeroRotator);
			}
		});
	});

	GI->ApiClient->Get(TEXT("/api/live-proxy/status"), CB);
}

void ATartariaForgeBuilding::AddProxyPrimitive(const FString& Type, FVector Location,
                                                FVector Scale, FRotator Rotation)
{
	// Determine which Engine basic shape to use
	const TCHAR* ShapePath = nullptr;
	if (Type == TEXT("box"))
	{
		ShapePath = TEXT("/Engine/BasicShapes/Cube");
	}
	else if (Type == TEXT("cylinder"))
	{
		ShapePath = TEXT("/Engine/BasicShapes/Cylinder");
	}
	else if (Type == TEXT("sphere"))
	{
		ShapePath = TEXT("/Engine/BasicShapes/Sphere");
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Forge '%s': Unknown proxy primitive type '%s'"), *BuildingName, *Type);
		return;
	}

	// Load the shape mesh
	UStaticMesh* ShapeMesh = LoadObject<UStaticMesh>(nullptr, ShapePath);
	if (!ShapeMesh)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Forge '%s': Failed to load shape mesh '%s'"), *BuildingName, ShapePath);
		return;
	}

	// Acquire a component from the pre-allocated pool instead of creating one
	UStaticMeshComponent* PoolComp = AcquireProxyPrimitive();
	if (!PoolComp) return;  // Pool exhausted — warning already logged

	// Re-attach to pedestal so proxy geometry appears on the display platform
	if (PedestalMesh)
	{
		PoolComp->AttachToComponent(PedestalMesh,
			FAttachmentTransformRules::KeepRelativeTransform);
	}
	else
	{
		PoolComp->AttachToComponent(RootComponent,
			FAttachmentTransformRules::KeepRelativeTransform);
	}

	PoolComp->SetStaticMesh(ShapeMesh);
	PoolComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Position relative to the pedestal top
	// Pedestal top is at approximately Z=40 (from DisplayModelMesh offset)
	FVector PedestalTopOffset = FVector(0.0f, 0.0f, 40.0f);
	PoolComp->SetRelativeLocation(PedestalTopOffset + Location);
	PoolComp->SetRelativeRotation(Rotation);

	// Start at zero scale — will animate up
	PoolComp->SetRelativeScale3D(FVector::ZeroVector);

	// Apply the translucent golden proxy material
	if (ProxyMaterial)
	{
		PoolComp->SetMaterial(0, ProxyMaterial);
	}
	else
	{
		// Fallback: create a per-component dynamic material
		UMaterialInstanceDynamic* DynMat = PoolComp->CreateAndSetMaterialInstanceDynamic(0);
		if (DynMat)
		{
			DynMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.9f, 0.75f, 0.2f));
			DynMat->SetScalarParameterValue(TEXT("Opacity"), 0.4f);
			DynMat->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(0.3f, 0.2f, 0.05f));
		}
	}

	int32 NewIndex = ProxyPrimitives.Num();
	ProxyPrimitives.Add(PoolComp);

	// Queue scale-up animation: 0 -> target over 0.3 seconds
	FProxyScaleAnim Anim;
	Anim.Index = NewIndex;
	Anim.TargetScale = Scale;
	Anim.Elapsed = 0.0f;
	Anim.Duration = 0.3f;
	ProxyScaleAnims.Add(Anim);

	UE_LOG(LogTemp, Verbose,
		TEXT("Forge '%s': Added proxy primitive %d (%s) scale=(%s)"),
		*BuildingName, NewIndex, *Type, *Scale.ToString());
}

void ATartariaForgeBuilding::ClearProxyGeometry()
{
	// Release all active proxies back to the pool instead of destroying them
	ReleaseAllProxies();
	ProxyPrimitives.Empty();
	ProxyScaleAnims.Empty();
	bProxyActive = false;
	bSolidifyActive = false;
	SolidifyElapsed = 0.0f;

	UE_LOG(LogTemp, Log, TEXT("Forge '%s': Proxy geometry cleared (pool released)"), *BuildingName);
}

void ATartariaForgeBuilding::SolidifyProxy()
{
	if (ProxyPrimitives.Num() == 0)
	{
		bProxyActive = false;
		return;
	}

	UE_LOG(LogTemp, Log,
		TEXT("Forge '%s': Solidifying %d proxy primitives"),
		*BuildingName, ProxyPrimitives.Num());

	bSolidifyActive = true;
	SolidifyElapsed = 0.0f;

	// Each proxy primitive gets its own dynamic material instance for independent lerping
	for (UStaticMeshComponent* Comp : ProxyPrimitives)
	{
		if (!Comp) continue;

		// Ensure each component has its own dynamic material (not shared ProxyMaterial)
		UMaterialInstanceDynamic* DynMat = Comp->CreateAndSetMaterialInstanceDynamic(0);
		if (DynMat)
		{
			// Start from current translucent gold state
			DynMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.9f, 0.75f, 0.2f));
			DynMat->SetScalarParameterValue(TEXT("Opacity"), 0.4f);
			DynMat->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(0.3f, 0.2f, 0.05f));
		}
	}
}
