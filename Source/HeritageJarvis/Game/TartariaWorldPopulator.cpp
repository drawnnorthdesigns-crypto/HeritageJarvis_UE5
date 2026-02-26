#include "TartariaWorldPopulator.h"
#include "TartariaBiomeVolume.h"
#include "TartariaDayNightCycle.h"
#include "TartariaResourceNode.h"
#include "TartariaForgeBuilding.h"
#include "TartariaNPC.h"
#include "TartariaTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

// -------------------------------------------------------
// Main entry point
// -------------------------------------------------------

void UTartariaWorldPopulator::PopulateWorld(UWorld* World)
{
	if (!World) return;

	UE_LOG(LogTemp, Log, TEXT("[WorldPopulator] Populating Tartaria world..."));

	SpawnBiomes(World);
	SpawnDayNightCycle(World);
	SpawnResourceNodes(World);
	SpawnBuildings(World);
	SpawnNPCs(World);

	UE_LOG(LogTemp, Log, TEXT("[WorldPopulator] World population complete."));
}

// -------------------------------------------------------
// Biome Volumes
// -------------------------------------------------------

void UTartariaWorldPopulator::SpawnBiomes(UWorld* World)
{
	struct FBiomeDef
	{
		FName Tag;
		FString BiomeKey;
		FVector Location;
		float Radius;
		int32 Difficulty;
	};

	const TArray<FBiomeDef> Biomes = {
		{ FName("Biome_CLEARINGHOUSE"),  TEXT("CLEARINGHOUSE"),  FVector(0.f, 0.f, 0.f),          150000.f, 1 },
		{ FName("Biome_SCRIPTORIUM"),    TEXT("SCRIPTORIUM"),    FVector(250000.f, 0.f, 0.f),     120000.f, 2 },
		{ FName("Biome_MONOLITH_WARD"),  TEXT("MONOLITH_WARD"), FVector(0.f, 300000.f, 0.f),     130000.f, 3 },
		{ FName("Biome_FORGE_DISTRICT"), TEXT("FORGE_DISTRICT"), FVector(-250000.f, 0.f, 0.f),   140000.f, 3 },
		{ FName("Biome_VOID_REACH"),     TEXT("VOID_REACH"),     FVector(0.f, -350000.f, 0.f),   160000.f, 5 },
	};

	for (const FBiomeDef& Def : Biomes)
	{
		if (HasActorWithTag(World, Def.Tag)) continue;

		FActorSpawnParameters Params;
		Params.Name = Def.Tag;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ATartariaBiomeVolume* Biome = World->SpawnActor<ATartariaBiomeVolume>(
			ATartariaBiomeVolume::StaticClass(), Def.Location, FRotator::ZeroRotator, Params);

		if (Biome)
		{
			Biome->BiomeKey = Def.BiomeKey;
			Biome->ZoneRadius = Def.Radius;
			Biome->Difficulty = Def.Difficulty;
			Biome->Tags.Add(Def.Tag);
			UE_LOG(LogTemp, Log, TEXT("[WorldPopulator] Spawned biome: %s at %s"),
				*Def.BiomeKey, *Def.Location.ToString());
		}
	}
}

// -------------------------------------------------------
// Day/Night Cycle
// -------------------------------------------------------

void UTartariaWorldPopulator::SpawnDayNightCycle(UWorld* World)
{
	const FName Tag("DayNightCycle");
	if (HasActorWithTag(World, Tag)) return;

	FActorSpawnParameters Params;
	Params.Name = Tag;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ATartariaDayNightCycle* DNC = World->SpawnActor<ATartariaDayNightCycle>(
		ATartariaDayNightCycle::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);

	if (DNC)
	{
		DNC->DayLengthSeconds = 600.f;
		DNC->TimeOfDay = 8.0f;
		DNC->Tags.Add(Tag);
		UE_LOG(LogTemp, Log, TEXT("[WorldPopulator] Spawned day/night cycle (10-min day, start 8:00 AM)"));
	}
}

// -------------------------------------------------------
// Resource Nodes
// -------------------------------------------------------

void UTartariaWorldPopulator::SpawnResourceNodes(UWorld* World)
{
	struct FResourceDef
	{
		FName Tag;
		ETartariaResourceType Type;
		FVector BiomeCenter;
		float BiomeRadius;
	};

	// 5 Iron in Forge District, 5 Stone in Monolith Ward,
	// 4 Knowledge in Scriptorium, 3 Crystal in Void Reach
	TArray<FResourceDef> Resources;

	const FVector ForgeCenter(-250000.f, 0.f, 0.f);
	const float ForgeRadius = 140000.f;
	for (int32 i = 0; i < 5; ++i)
	{
		Resources.Add({
			*FString::Printf(TEXT("Resource_Iron_%d"), i),
			ETartariaResourceType::Iron, ForgeCenter, ForgeRadius
		});
	}

	const FVector MonolithCenter(0.f, 300000.f, 0.f);
	const float MonolithRadius = 130000.f;
	for (int32 i = 0; i < 5; ++i)
	{
		Resources.Add({
			*FString::Printf(TEXT("Resource_Stone_%d"), i),
			ETartariaResourceType::Stone, MonolithCenter, MonolithRadius
		});
	}

	const FVector ScriptoriumCenter(250000.f, 0.f, 0.f);
	const float ScriptoriumRadius = 120000.f;
	for (int32 i = 0; i < 4; ++i)
	{
		Resources.Add({
			*FString::Printf(TEXT("Resource_Knowledge_%d"), i),
			ETartariaResourceType::Knowledge, ScriptoriumCenter, ScriptoriumRadius
		});
	}

	const FVector VoidCenter(0.f, -350000.f, 0.f);
	const float VoidRadius = 160000.f;
	for (int32 i = 0; i < 3; ++i)
	{
		Resources.Add({
			*FString::Printf(TEXT("Resource_Crystal_%d"), i),
			ETartariaResourceType::Crystal, VoidCenter, VoidRadius
		});
	}

	int32 SpawnCount = 0;
	for (const FResourceDef& Def : Resources)
	{
		if (HasActorWithTag(World, Def.Tag)) continue;

		FVector SpawnLoc = RandomOffsetInRadius(Def.BiomeCenter, Def.BiomeRadius);

		FActorSpawnParameters Params;
		Params.Name = Def.Tag;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		ATartariaResourceNode* Node = World->SpawnActor<ATartariaResourceNode>(
			ATartariaResourceNode::StaticClass(), SpawnLoc, FRotator::ZeroRotator, Params);

		if (Node)
		{
			Node->ResourceType = Def.Type;
			Node->Tags.Add(Def.Tag);

			// Per-type color
			switch (Def.Type)
			{
			case ETartariaResourceType::Iron:
				SetMeshColor(Node->ResourceMesh, FLinearColor(0.6f, 0.3f, 0.1f));
				break;
			case ETartariaResourceType::Stone:
				SetMeshColor(Node->ResourceMesh, FLinearColor(0.5f, 0.5f, 0.5f));
				break;
			case ETartariaResourceType::Knowledge:
				SetMeshColor(Node->ResourceMesh, FLinearColor(0.2f, 0.4f, 0.9f));
				break;
			case ETartariaResourceType::Crystal:
				SetMeshColor(Node->ResourceMesh, FLinearColor(0.7f, 0.2f, 0.8f));
				break;
			}

			++SpawnCount;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[WorldPopulator] Spawned %d resource nodes"), SpawnCount);
}

// -------------------------------------------------------
// Buildings
// -------------------------------------------------------

void UTartariaWorldPopulator::SpawnBuildings(UWorld* World)
{
	struct FBuildingDef
	{
		FName Tag;
		FString Name;
		FString Id;
		ETartariaBuildingType Type;
		FVector Location;
	};

	const TArray<FBuildingDef> Buildings = {
		{ FName("Building_Forge"),        TEXT("The Great Forge"),     TEXT("forge_main"),       ETartariaBuildingType::Forge,       FVector(-250000.f, 0.f, 0.f) },
		{ FName("Building_Scriptorium"),  TEXT("Scriptorium Hall"),   TEXT("scriptorium_main"), ETartariaBuildingType::Scriptorium, FVector(250000.f, 0.f, 0.f) },
		{ FName("Building_Treasury"),     TEXT("The Treasury"),       TEXT("treasury_main"),    ETartariaBuildingType::Treasury,    FVector(0.f, 0.f, 0.f) },
		{ FName("Building_Barracks"),     TEXT("The Barracks"),       TEXT("barracks_main"),    ETartariaBuildingType::Barracks,    FVector(0.f, 300000.f, 0.f) },
		{ FName("Building_VoidLab"),      TEXT("Void Laboratory"),    TEXT("void_lab"),         ETartariaBuildingType::Lab,         FVector(0.f, -350000.f, 0.f) },
	};

	int32 SpawnCount = 0;
	for (const FBuildingDef& Def : Buildings)
	{
		if (HasActorWithTag(World, Def.Tag)) continue;

		FActorSpawnParameters Params;
		Params.Name = Def.Tag;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		ATartariaForgeBuilding* Bldg = World->SpawnActor<ATartariaForgeBuilding>(
			ATartariaForgeBuilding::StaticClass(), Def.Location, FRotator::ZeroRotator, Params);

		if (Bldg)
		{
			Bldg->BuildingId = Def.Id;
			Bldg->BuildingName = Def.Name;
			Bldg->BuildingType = Def.Type;
			Bldg->Tags.Add(Def.Tag);

			// Per-building color and scale
			switch (Def.Type)
			{
			case ETartariaBuildingType::Forge:
				SetMeshColor(Bldg->BuildingMesh, FLinearColor(1.0f, 0.5f, 0.0f));
				Bldg->BuildingMesh->SetWorldScale3D(FVector(3.f, 3.f, 5.f));
				break;
			case ETartariaBuildingType::Scriptorium:
				SetMeshColor(Bldg->BuildingMesh, FLinearColor(0.2f, 0.3f, 0.8f));
				Bldg->BuildingMesh->SetWorldScale3D(FVector(4.f, 4.f, 3.f));
				break;
			case ETartariaBuildingType::Treasury:
				SetMeshColor(Bldg->BuildingMesh, FLinearColor(0.9f, 0.8f, 0.2f));
				Bldg->BuildingMesh->SetWorldScale3D(FVector(3.f, 3.f, 4.f));
				break;
			case ETartariaBuildingType::Barracks:
				SetMeshColor(Bldg->BuildingMesh, FLinearColor(0.3f, 0.5f, 0.2f));
				Bldg->BuildingMesh->SetWorldScale3D(FVector(5.f, 3.f, 3.f));
				break;
			case ETartariaBuildingType::Lab:
				SetMeshColor(Bldg->BuildingMesh, FLinearColor(0.4f, 0.1f, 0.6f));
				Bldg->BuildingMesh->SetWorldScale3D(FVector(3.f, 3.f, 6.f));
				break;
			}

			++SpawnCount;
			UE_LOG(LogTemp, Log, TEXT("[WorldPopulator] Spawned building: %s"), *Def.Name);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[WorldPopulator] Spawned %d buildings"), SpawnCount);
}

// -------------------------------------------------------
// NPCs
// -------------------------------------------------------

void UTartariaWorldPopulator::SpawnNPCs(UWorld* World)
{
	struct FNPCDef
	{
		FName Tag;
		FString Name;
		FString Faction;
		FString Persona;
		FVector Location;
	};

	const TArray<FNPCDef> NPCs = {
		{
			FName("NPC_TheSteward"),
			TEXT("The Steward"),
			TEXT("STEWARDS"),
			TEXT("You are The Steward, wise overseer of Tartaria. Speak with authority about engineering and project management."),
			FVector(5000.f, 0.f, 0.f)
		},
		{
			FName("NPC_TheArchivist"),
			TEXT("The Archivist"),
			TEXT("ARCHIVISTS"),
			TEXT("You are The Archivist, keeper of all knowledge in Tartaria. Speak with reverence about history and documentation."),
			FVector(250000.f, 5000.f, 0.f)
		},
		{
			FName("NPC_TheMason"),
			TEXT("The Mason"),
			TEXT("MASONS"),
			TEXT("You are The Mason, master builder of Monolith Ward. Speak gruffly about construction and stonework."),
			FVector(5000.f, 300000.f, 0.f)
		},
		{
			FName("NPC_TheComptroller"),
			TEXT("The Comptroller"),
			TEXT("COMPTROLLERS"),
			TEXT("You are The Comptroller, guardian of the treasury. Speak precisely about trade, economy, and resource management."),
			FVector(-5000.f, 0.f, 0.f)
		},
		{
			FName("NPC_TheVoidWalker"),
			TEXT("The Void Walker"),
			TEXT("VOID_WALKERS"),
			TEXT("You are The Void Walker, enigmatic explorer of the Void Reach. Speak cryptically about mysteries and crystals."),
			FVector(0.f, -345000.f, 0.f)
		},
	};

	int32 SpawnCount = 0;
	for (const FNPCDef& Def : NPCs)
	{
		if (HasActorWithTag(World, Def.Tag)) continue;

		FActorSpawnParameters Params;
		Params.Name = Def.Tag;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		ATartariaNPC* NPC = World->SpawnActor<ATartariaNPC>(
			ATartariaNPC::StaticClass(), Def.Location, FRotator::ZeroRotator, Params);

		if (NPC)
		{
			NPC->NPCName = Def.Name;
			NPC->FactionKey = Def.Faction;
			NPC->PersonaPrompt = Def.Persona;
			NPC->Tags.Add(Def.Tag);

			// Per-NPC color on body mesh
			if (Def.Faction == TEXT("STEWARDS"))
				SetMeshColor(NPC->BodyMesh, FLinearColor(0.9f, 0.9f, 0.9f));
			else if (Def.Faction == TEXT("ARCHIVISTS"))
				SetMeshColor(NPC->BodyMesh, FLinearColor(0.3f, 0.4f, 0.8f));
			else if (Def.Faction == TEXT("MASONS"))
				SetMeshColor(NPC->BodyMesh, FLinearColor(0.5f, 0.35f, 0.2f));
			else if (Def.Faction == TEXT("COMPTROLLERS"))
				SetMeshColor(NPC->BodyMesh, FLinearColor(0.8f, 0.7f, 0.2f));
			else if (Def.Faction == TEXT("VOID_WALKERS"))
				SetMeshColor(NPC->BodyMesh, FLinearColor(0.5f, 0.1f, 0.7f));

			++SpawnCount;
			UE_LOG(LogTemp, Log, TEXT("[WorldPopulator] Spawned NPC: %s (%s)"), *Def.Name, *Def.Faction);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[WorldPopulator] Spawned %d NPCs"), SpawnCount);
}

// -------------------------------------------------------
// Helpers
// -------------------------------------------------------

bool UTartariaWorldPopulator::HasActorWithTag(UWorld* World, FName Tag) const
{
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->Tags.Contains(Tag))
			return true;
	}
	return false;
}

FVector UTartariaWorldPopulator::RandomOffsetInRadius(const FVector& Center, float Radius) const
{
	// Place within 70% of radius to avoid center/edge clustering
	const float MaxDist = Radius * 0.7f;
	const float Angle = FMath::FRandRange(0.f, 2.f * PI);
	const float Dist = FMath::FRandRange(MaxDist * 0.3f, MaxDist);

	return Center + FVector(
		FMath::Cos(Angle) * Dist,
		FMath::Sin(Angle) * Dist,
		0.f
	);
}

void UTartariaWorldPopulator::SetMeshColor(UStaticMeshComponent* Mesh, const FLinearColor& Color)
{
	if (!Mesh) return;
	UMaterialInstanceDynamic* DynMat = Mesh->CreateAndSetMaterialInstanceDynamic(0);
	if (DynMat)
		DynMat->SetVectorParameterValue(TEXT("Color"), Color);
}
