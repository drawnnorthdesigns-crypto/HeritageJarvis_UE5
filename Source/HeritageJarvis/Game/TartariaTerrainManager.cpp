#include "TartariaTerrainManager.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ATartariaTerrainManager::ATartariaTerrainManager()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void ATartariaTerrainManager::SpawnTerrainPlanes(UWorld* World)
{
	if (!World) return;

	/*
	 * Ground plane per biome — each gets a distinct earth tone.
	 *
	 * Engine/BasicShapes/Plane is 100x100 UU by default.
	 * Biome radii are 120k-160k UU, so we scale the plane to cover the zone.
	 * Scale factor = (Radius * 2) / 100 = Radius * 0.02
	 *
	 * Colors chosen to reinforce biome identity:
	 *   CLEARINGHOUSE:  Warm sandy earth (safe, inviting)
	 *   SCRIPTORIUM:    Cool gray stone (library pavements)
	 *   MONOLITH_WARD:  Dark slate (heavy, oppressive)
	 *   FORGE_DISTRICT: Burnt sienna (industrial, ashen)
	 *   VOID_REACH:     Near-black teal (alien, corrupted)
	 */

	struct FTerrainDef
	{
		FName Tag;
		FVector Center;
		float Radius;
		FLinearColor GroundColor;
		float Roughness;
	};

	const TArray<FTerrainDef> Terrains = {
		{
			FName("Terrain_CLEARINGHOUSE"),
			FVector(0.f, 0.f, -10.f),
			150000.f,
			FLinearColor(0.45f, 0.38f, 0.25f),  // Warm sandy earth
			0.85f
		},
		{
			FName("Terrain_SCRIPTORIUM"),
			FVector(250000.f, 0.f, -10.f),
			120000.f,
			FLinearColor(0.35f, 0.35f, 0.4f),   // Cool gray stone
			0.75f
		},
		{
			FName("Terrain_MONOLITH_WARD"),
			FVector(0.f, 300000.f, -10.f),
			130000.f,
			FLinearColor(0.18f, 0.18f, 0.22f),  // Dark slate
			0.9f
		},
		{
			FName("Terrain_FORGE_DISTRICT"),
			FVector(-250000.f, 0.f, -10.f),
			140000.f,
			FLinearColor(0.35f, 0.2f, 0.12f),   // Burnt sienna
			0.8f
		},
		{
			FName("Terrain_VOID_REACH"),
			FVector(0.f, -350000.f, -10.f),
			160000.f,
			FLinearColor(0.08f, 0.12f, 0.1f),   // Near-black teal
			0.95f
		},
	};

	int32 SpawnCount = 0;
	for (const FTerrainDef& Def : Terrains)
	{
		// Check for existing terrain with this tag
		bool bExists = false;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->Tags.Contains(Def.Tag))
			{
				bExists = true;
				break;
			}
		}
		if (bExists) continue;

		SpawnGroundPlane(World, Def.Tag, Def.Center, Def.Radius, Def.GroundColor, Def.Roughness);
		++SpawnCount;
	}

	UE_LOG(LogTemp, Log, TEXT("[TerrainManager] Spawned %d terrain ground planes"), SpawnCount);
}

void ATartariaTerrainManager::SpawnGroundPlane(UWorld* World, const FName& Tag,
	const FVector& Center, float Radius, const FLinearColor& GroundColor, float Roughness)
{
	// Spawn a simple actor with a scaled plane mesh
	FActorSpawnParameters Params;
	Params.Name = Tag;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* PlaneActor = World->SpawnActor<AActor>(AActor::StaticClass(), Center, FRotator::ZeroRotator, Params);
	if (!PlaneActor) return;

	PlaneActor->Tags.Add(Tag);

	// Create root scene component
	USceneComponent* PlaneRoot = NewObject<USceneComponent>(PlaneActor, TEXT("PlaneRoot"));
	PlaneActor->SetRootComponent(PlaneRoot);
	PlaneRoot->RegisterComponent();

	// Create the ground mesh component
	UStaticMeshComponent* GroundMesh = NewObject<UStaticMeshComponent>(PlaneActor, TEXT("GroundMesh"));
	GroundMesh->SetupAttachment(PlaneRoot);

	UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Engine/BasicShapes/Plane.Plane"));

	if (PlaneMesh)
	{
		GroundMesh->SetStaticMesh(PlaneMesh);

		// Scale: Plane is 100x100 UU → scale to cover biome diameter
		// Diameter = Radius * 2, so scale = (Radius * 2) / 100
		float ScaleXY = (Radius * 2.0f) / 100.0f;
		GroundMesh->SetRelativeScale3D(FVector(ScaleXY, ScaleXY, 1.0f));

		// Set ground material
		UMaterialInstanceDynamic* DynMat = GroundMesh->CreateAndSetMaterialInstanceDynamic(0);
		if (DynMat)
		{
			DynMat->SetVectorParameterValue(TEXT("Color"), GroundColor);
		}

		// Enable collision so the player can walk on it
		GroundMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		GroundMesh->SetCollisionResponseToAllChannels(ECR_Block);
		GroundMesh->SetCollisionObjectType(ECC_WorldStatic);

		// Shadows from terrain
		GroundMesh->CastShadow = false; // Ground doesn't need to cast shadows
		GroundMesh->bReceivesDecals = true;
	}

	GroundMesh->RegisterComponent();

	UE_LOG(LogTemp, Log, TEXT("[TerrainManager] Spawned ground plane: %s at %s (radius=%.0f)"),
		*Tag.ToString(), *Center.ToString(), Radius);
}
