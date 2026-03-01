#include "TartariaWorldPopulator.h"
#include "TartariaBiomeVolume.h"
#include "TartariaDayNightCycle.h"
#include "TartariaResourceNode.h"
#include "TartariaForgeBuilding.h"
#include "TartariaTerminal.h"
#include "TartariaNPC.h"
#include "TartariaQuestMarker.h"
#include "TartariaAmbientSound.h"
#include "TartariaBiomeParticles.h"
#include "TartariaTerrainManager.h"
#include "TartariaInstancedWealth.h"
#include "TartariaPatentRegistry.h"
#include "TartariaAlchemicalScales.h"
#include "TartariaFactionBanner.h"
#include "TartariaLandmark.h"
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

	SpawnTerrain(World);  // Terrain first — everything spawns on top
	SpawnBiomes(World);
	SpawnDayNightCycle(World);
	SpawnResourceNodes(World);
	SpawnBuildings(World);
	SpawnTerminals(World);
	SpawnNPCs(World);
	SpawnQuestMarkers(World);
	SpawnAmbientSounds(World);
	SpawnBiomeParticles(World);
	SpawnInstancedWealth(World);
	SpawnPatentRegistry(World);
	SpawnAlchemicalScales(World);
	SpawnFactionBanners(World);
	SpawnLandmarks(World);

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

			// Configure per-biome post-process atmosphere (color grading, bloom, vignette)
			Biome->ConfigureBiomeAtmosphere();

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

	// Resource distribution: mixed types per biome for density
	TArray<FResourceDef> Resources;

	// Biome centers and radii
	const FVector ClearingCenter(0.f, 0.f, 0.f);
	const float ClearingRadius = 150000.f;
	const FVector ForgeCenter(-250000.f, 0.f, 0.f);
	const float ForgeRadius = 140000.f;
	const FVector MonolithCenter(0.f, 300000.f, 0.f);
	const float MonolithRadius = 130000.f;
	const FVector ScriptoriumCenter(250000.f, 0.f, 0.f);
	const float ScriptoriumRadius = 120000.f;
	const FVector VoidCenter(0.f, -350000.f, 0.f);
	const float VoidRadius = 160000.f;

	// Clearinghouse: starter zone — iron + stone (easy gathering)
	for (int32 i = 0; i < 4; ++i)
		Resources.Add({ *FString::Printf(TEXT("Resource_CH_Iron_%d"), i),
			ETartariaResourceType::Iron, ClearingCenter, ClearingRadius });
	for (int32 i = 0; i < 3; ++i)
		Resources.Add({ *FString::Printf(TEXT("Resource_CH_Stone_%d"), i),
			ETartariaResourceType::Stone, ClearingCenter, ClearingRadius });

	// Forge District: iron-heavy + some stone
	for (int32 i = 0; i < 6; ++i)
		Resources.Add({ *FString::Printf(TEXT("Resource_Iron_%d"), i),
			ETartariaResourceType::Iron, ForgeCenter, ForgeRadius });
	for (int32 i = 0; i < 3; ++i)
		Resources.Add({ *FString::Printf(TEXT("Resource_FD_Stone_%d"), i),
			ETartariaResourceType::Stone, ForgeCenter, ForgeRadius });

	// Monolith Ward: stone-heavy + crystal veins
	for (int32 i = 0; i < 6; ++i)
		Resources.Add({ *FString::Printf(TEXT("Resource_Stone_%d"), i),
			ETartariaResourceType::Stone, MonolithCenter, MonolithRadius });
	for (int32 i = 0; i < 2; ++i)
		Resources.Add({ *FString::Printf(TEXT("Resource_MW_Crystal_%d"), i),
			ETartariaResourceType::Crystal, MonolithCenter, MonolithRadius });

	// Scriptorium: knowledge-heavy + crystal catalysts
	for (int32 i = 0; i < 5; ++i)
		Resources.Add({ *FString::Printf(TEXT("Resource_Knowledge_%d"), i),
			ETartariaResourceType::Knowledge, ScriptoriumCenter, ScriptoriumRadius });
	for (int32 i = 0; i < 2; ++i)
		Resources.Add({ *FString::Printf(TEXT("Resource_SC_Crystal_%d"), i),
			ETartariaResourceType::Crystal, ScriptoriumCenter, ScriptoriumRadius });

	// Void Reach: crystal-heavy + knowledge fragments
	for (int32 i = 0; i < 5; ++i)
		Resources.Add({ *FString::Printf(TEXT("Resource_Crystal_%d"), i),
			ETartariaResourceType::Crystal, VoidCenter, VoidRadius });
	for (int32 i = 0; i < 3; ++i)
		Resources.Add({ *FString::Printf(TEXT("Resource_VR_Knowledge_%d"), i),
			ETartariaResourceType::Knowledge, VoidCenter, VoidRadius });

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
		// Primary buildings (one per biome)
		{ FName("Building_Treasury"),     TEXT("The Treasury"),       TEXT("treasury_main"),    ETartariaBuildingType::Treasury,    FVector(0.f, 0.f, 0.f) },
		{ FName("Building_Forge"),        TEXT("The Great Forge"),     TEXT("forge_main"),       ETartariaBuildingType::Forge,       FVector(-250000.f, 0.f, 0.f) },
		{ FName("Building_Scriptorium"),  TEXT("Scriptorium Hall"),   TEXT("scriptorium_main"), ETartariaBuildingType::Scriptorium, FVector(250000.f, 0.f, 0.f) },
		{ FName("Building_Barracks"),     TEXT("The Barracks"),       TEXT("barracks_main"),    ETartariaBuildingType::Barracks,    FVector(0.f, 300000.f, 0.f) },
		{ FName("Building_VoidLab"),      TEXT("Void Laboratory"),    TEXT("void_lab"),         ETartariaBuildingType::Lab,         FVector(0.f, -350000.f, 0.f) },

		// Secondary buildings — smaller outposts per biome
		{ FName("Building_Watchtower"),   TEXT("Watchtower"),          TEXT("watchtower"),       ETartariaBuildingType::Barracks,    FVector(80000.f, 40000.f, 0.f) },
		{ FName("Building_Smelter"),      TEXT("The Smelter"),         TEXT("smelter"),          ETartariaBuildingType::Forge,       FVector(-200000.f, 60000.f, 0.f) },
		{ FName("Building_LibraryAnnex"), TEXT("Library Annex"),       TEXT("library_annex"),    ETartariaBuildingType::Scriptorium, FVector(200000.f, 50000.f, 0.f) },
		{ FName("Building_QuarryOffice"),TEXT("Quarry Office"),       TEXT("quarry_office"),    ETartariaBuildingType::Treasury,    FVector(50000.f, 250000.f, 0.f) },
		{ FName("Building_CrystalSpire"),TEXT("Crystal Spire"),       TEXT("crystal_spire"),    ETartariaBuildingType::Lab,         FVector(40000.f, -300000.f, 0.f) },
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

			// Detect secondary buildings (smaller scale multiplier)
			bool bSecondary = Def.Id != TEXT("forge_main")
				&& Def.Id != TEXT("scriptorium_main")
				&& Def.Id != TEXT("treasury_main")
				&& Def.Id != TEXT("barracks_main")
				&& Def.Id != TEXT("void_lab");
			float ScaleMul = bSecondary ? 0.6f : 1.0f;

			// Color the base mesh per building type
			switch (Def.Type)
			{
			case ETartariaBuildingType::Forge:
				SetMeshColor(Bldg->BuildingMesh, FLinearColor(1.0f, 0.5f, 0.0f));
				break;
			case ETartariaBuildingType::Scriptorium:
				SetMeshColor(Bldg->BuildingMesh, FLinearColor(0.2f, 0.3f, 0.8f));
				break;
			case ETartariaBuildingType::Treasury:
				SetMeshColor(Bldg->BuildingMesh, FLinearColor(0.9f, 0.8f, 0.2f));
				break;
			case ETartariaBuildingType::Barracks:
				SetMeshColor(Bldg->BuildingMesh, FLinearColor(0.3f, 0.5f, 0.2f));
				break;
			case ETartariaBuildingType::Lab:
				SetMeshColor(Bldg->BuildingMesh, FLinearColor(0.4f, 0.1f, 0.6f));
				break;
			}

			// Build compound architectural silhouette (chimney, dome, pillars, etc.)
			Bldg->SetupBuildingVisuals(ScaleMul);

			++SpawnCount;
			UE_LOG(LogTemp, Log, TEXT("[WorldPopulator] Spawned building: %s%s"),
				*Def.Name, bSecondary ? TEXT(" (secondary)") : TEXT(""));
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
		ETartariaSpecialist Specialist;
		FString Persona;
		FVector Location;
		FLinearColor Color;
	};

	const TArray<FNPCDef> NPCs = {
		{
			FName("NPC_TheSteward"),
			TEXT("The Steward"),
			TEXT("STEWARDS"),
			ETartariaSpecialist::Governor,
			TEXT("You are The Steward, wise Governor of Tartaria. You oversee all governance, territorial claims, and faction diplomacy. Speak with calm authority about statecraft and the balance of power."),
			FVector(5000.f, 0.f, 0.f),
			FLinearColor(0.9f, 0.9f, 0.9f)
		},
		{
			FName("NPC_TheArchivist"),
			TEXT("The Archivist"),
			TEXT("ARCHIVISTS"),
			ETartariaSpecialist::Scribe,
			TEXT("You are The Archivist, Scribe of the Scriptorium. You maintain all records, research manuscripts, and the tech tree. Speak with reverence about knowledge, history, and the pursuit of understanding."),
			FVector(250000.f, 5000.f, 0.f),
			FLinearColor(0.3f, 0.4f, 0.8f)
		},
		{
			FName("NPC_TheMason"),
			TEXT("The Mason"),
			TEXT("MASONS"),
			ETartariaSpecialist::Surveyor,
			TEXT("You are The Mason, Surveyor of Monolith Ward. You oversee construction, mining operations, and territorial surveys. Speak gruffly about stonework, resource extraction, and building foundations."),
			FVector(5000.f, 300000.f, 0.f),
			FLinearColor(0.5f, 0.35f, 0.2f)
		},
		{
			FName("NPC_TheComptroller"),
			TEXT("The Comptroller"),
			TEXT("COMPTROLLERS"),
			ETartariaSpecialist::Steward,
			TEXT("You are The Comptroller, chief Steward of the Treasury. You manage credits, trade routes, and economic policy. Speak precisely about trade, supply chains, and resource management."),
			FVector(-5000.f, 0.f, 0.f),
			FLinearColor(0.8f, 0.7f, 0.2f)
		},
		{
			FName("NPC_TheVoidWalker"),
			TEXT("The Void Walker"),
			TEXT("VOID_WALKERS"),
			ETartariaSpecialist::WarCaptain,
			TEXT("You are The Void Walker, War Captain of the Void Reach. You command fleets, plan raids, and defend against incursions. Speak cryptically but fiercely about combat, threats, and the void beyond."),
			FVector(0.f, -345000.f, 0.f),
			FLinearColor(0.5f, 0.1f, 0.7f)
		},
		{
			FName("NPC_TheAlchemist"),
			TEXT("The Alchemist"),
			TEXT("ALCHEMISTS"),
			ETartariaSpecialist::Alchemist,
			TEXT("You are The Alchemist, master of transmutation and the laboratory. You research compounds, run experiments, and unlock alchemical secrets. Speak with wonder about discovery and the nature of matter."),
			FVector(-200000.f, 200000.f, 0.f),
			FLinearColor(0.2f, 0.8f, 0.6f)
		},
		{
			FName("NPC_TheForgeMaster"),
			TEXT("The Forge Master"),
			TEXT("FORGE_MASTERS"),
			ETartariaSpecialist::ForgeMaster,
			TEXT("You are The Forge Master, keeper of the Great Forge. You craft weapons, armor, and artifacts from raw materials. Speak with fiery passion about metallurgy, smithing, and the art of creation."),
			FVector(-245000.f, 5000.f, 0.f),
			FLinearColor(1.0f, 0.4f, 0.1f)
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
			NPC->SpecialistType = Def.Specialist;
			NPC->PersonaPrompt = Def.Persona;
			NPC->Tags.Add(Def.Tag);

			// Reapply appearance now that SpecialistType is set
			// (BeginPlay may have fired with default Steward type during SpawnActor)
			NPC->ApplySpecialistAppearance();

			++SpawnCount;
			UE_LOG(LogTemp, Log, TEXT("[WorldPopulator] Spawned NPC: %s (%s, %d)"),
				*Def.Name, *Def.Faction, static_cast<int32>(Def.Specialist));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[WorldPopulator] Spawned %d NPCs"), SpawnCount);
}

// -------------------------------------------------------
// Diegetic Terminals (3D spatial UI)
// -------------------------------------------------------

void UTartariaWorldPopulator::SpawnTerminals(UWorld* World)
{
	struct FTerminalDef
	{
		FName Tag;
		FString Name;
		ETartariaBuildingType Type;
		FVector Location;
		FRotator Rotation;
	};

	// Place a terminal 200 units in front of each primary building
	const TArray<FTerminalDef> Terminals = {
		{ FName("Terminal_Treasury"),     TEXT("Treasury Console"),    ETartariaBuildingType::Treasury,    FVector(200.f, 0.f, 0.f),        FRotator(0.f, 180.f, 0.f) },
		{ FName("Terminal_Forge"),        TEXT("Forge Terminal"),      ETartariaBuildingType::Forge,       FVector(-249800.f, 0.f, 0.f),    FRotator(0.f, 180.f, 0.f) },
		{ FName("Terminal_Scriptorium"),  TEXT("Archive Terminal"),    ETartariaBuildingType::Scriptorium, FVector(250200.f, 0.f, 0.f),     FRotator(0.f, 180.f, 0.f) },
		{ FName("Terminal_Barracks"),     TEXT("Barracks Terminal"),   ETartariaBuildingType::Barracks,    FVector(200.f, 300000.f, 0.f),   FRotator(0.f, 180.f, 0.f) },
		{ FName("Terminal_Lab"),          TEXT("Void Lab Terminal"),   ETartariaBuildingType::Lab,         FVector(200.f, -350000.f, 0.f),  FRotator(0.f, 180.f, 0.f) },
	};

	int32 SpawnCount = 0;
	for (const FTerminalDef& Def : Terminals)
	{
		if (HasActorWithTag(World, Def.Tag)) continue;

		FActorSpawnParameters Params;
		Params.Name = Def.Tag;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		ATartariaTerminal* Terminal = World->SpawnActor<ATartariaTerminal>(
			ATartariaTerminal::StaticClass(), Def.Location, Def.Rotation, Params);

		if (Terminal)
		{
			Terminal->TerminalName = Def.Name;
			Terminal->TerminalType = Def.Type;
			Terminal->Tags.Add(Def.Tag);

			// Color the frame mesh to match terminal type
			FLinearColor FrameColor;
			switch (Def.Type)
			{
			case ETartariaBuildingType::Forge:       FrameColor = FLinearColor(0.6f, 0.3f, 0.1f); break;
			case ETartariaBuildingType::Scriptorium: FrameColor = FLinearColor(0.2f, 0.3f, 0.6f); break;
			case ETartariaBuildingType::Treasury:    FrameColor = FLinearColor(0.6f, 0.5f, 0.2f); break;
			case ETartariaBuildingType::Barracks:    FrameColor = FLinearColor(0.4f, 0.15f, 0.15f); break;
			case ETartariaBuildingType::Lab:         FrameColor = FLinearColor(0.3f, 0.2f, 0.5f); break;
			default:                                 FrameColor = FLinearColor(0.5f, 0.5f, 0.5f); break;
			}
			SetMeshColor(Terminal->FrameMesh, FrameColor);

			SpawnCount++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[WorldPopulator] Spawned %d diegetic terminals"), SpawnCount);
}

// -------------------------------------------------------
// Quest Markers
// -------------------------------------------------------

void UTartariaWorldPopulator::SpawnQuestMarkers(UWorld* World)
{
	struct FQuestDef
	{
		FName Tag;
		FString QuestId;
		FString Title;
		FString Type;
		FString Biome;
		FVector Location;
		FLinearColor LightColor;
	};

	const TArray<FQuestDef> Quests = {
		// Clearinghouse — starter quests
		{
			FName("Quest_FirstDecree"),
			TEXT("quest_first_decree"), TEXT("The First Decree"),
			TEXT("exploration"), TEXT("CLEARINGHOUSE"),
			FVector(15000.f, 10000.f, 200.f),
			FLinearColor(1.0f, 0.9f, 0.3f)  // Gold
		},
		{
			FName("Quest_GuildRegistration"),
			TEXT("quest_guild_reg"), TEXT("Guild Registration"),
			TEXT("faction"), TEXT("CLEARINGHOUSE"),
			FVector(-10000.f, 20000.f, 200.f),
			FLinearColor(0.3f, 0.8f, 0.4f)  // Green
		},

		// Scriptorium — knowledge quests
		{
			FName("Quest_LostManuscript"),
			TEXT("quest_lost_manuscript"), TEXT("Lost Manuscript"),
			TEXT("event"), TEXT("SCRIPTORIUM"),
			FVector(230000.f, 30000.f, 200.f),
			FLinearColor(0.3f, 0.5f, 1.0f)  // Blue
		},
		{
			FName("Quest_AncientArchive"),
			TEXT("quest_ancient_archive"), TEXT("Ancient Archive"),
			TEXT("exploration"), TEXT("SCRIPTORIUM"),
			FVector(280000.f, -20000.f, 200.f),
			FLinearColor(0.7f, 0.5f, 1.0f)  // Purple
		},

		// Monolith Ward — construction/threat quests
		{
			FName("Quest_MonolithMystery"),
			TEXT("quest_monolith_mystery"), TEXT("Monolith Mystery"),
			TEXT("exploration"), TEXT("MONOLITH_WARD"),
			FVector(30000.f, 280000.f, 200.f),
			FLinearColor(0.6f, 0.6f, 0.6f)  // Gray
		},
		{
			FName("Quest_StoneGiant"),
			TEXT("quest_stone_giant"), TEXT("Stone Giant Sighting"),
			TEXT("threat"), TEXT("MONOLITH_WARD"),
			FVector(-30000.f, 320000.f, 200.f),
			FLinearColor(0.9f, 0.3f, 0.2f)  // Red
		},

		// Forge District — crafting quests
		{
			FName("Quest_SmithChallenge"),
			TEXT("quest_smith_challenge"), TEXT("Master Smith's Challenge"),
			TEXT("faction"), TEXT("FORGE_DISTRICT"),
			FVector(-230000.f, 40000.f, 200.f),
			FLinearColor(1.0f, 0.6f, 0.1f)  // Orange
		},
		{
			FName("Quest_OreVein"),
			TEXT("quest_ore_vein"), TEXT("Ore Vein Discovery"),
			TEXT("event"), TEXT("FORGE_DISTRICT"),
			FVector(-270000.f, -30000.f, 200.f),
			FLinearColor(0.7f, 0.4f, 0.2f)  // Brown
		},

		// Void Reach — danger quests
		{
			FName("Quest_VoidAnomaly"),
			TEXT("quest_void_anomaly"), TEXT("Void Anomaly"),
			TEXT("threat"), TEXT("VOID_REACH"),
			FVector(30000.f, -330000.f, 200.f),
			FLinearColor(0.9f, 0.1f, 0.3f)  // Red
		},
		{
			FName("Quest_CrystalConverge"),
			TEXT("quest_crystal_convergence"), TEXT("Crystal Convergence"),
			TEXT("exploration"), TEXT("VOID_REACH"),
			FVector(-40000.f, -370000.f, 200.f),
			FLinearColor(0.6f, 0.2f, 0.9f)  // Violet
		},
	};

	int32 SpawnCount = 0;
	for (const FQuestDef& Def : Quests)
	{
		if (HasActorWithTag(World, Def.Tag)) continue;

		FActorSpawnParameters Params;
		Params.Name = Def.Tag;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ATartariaQuestMarker* Marker = World->SpawnActor<ATartariaQuestMarker>(
			ATartariaQuestMarker::StaticClass(), Def.Location, FRotator::ZeroRotator, Params);

		if (Marker)
		{
			Marker->QuestId = Def.QuestId;
			Marker->QuestTitle = Def.Title;
			Marker->QuestType = Def.Type;
			Marker->LinkedBiome = Def.Biome;
			Marker->Tags.Add(Def.Tag);

			// Color the light per quest type
			if (Marker->MarkerLight)
				Marker->MarkerLight->SetLightColor(Def.LightColor);

			++SpawnCount;
			UE_LOG(LogTemp, Log, TEXT("[WorldPopulator] Spawned quest: %s (%s) in %s"),
				*Def.Title, *Def.Type, *Def.Biome);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[WorldPopulator] Spawned %d quest markers"), SpawnCount);
}

// -------------------------------------------------------
// Instanced Wealth (Treasury gold bars via HISM)
// -------------------------------------------------------

void UTartariaWorldPopulator::SpawnInstancedWealth(UWorld* World)
{
	const FName Tag("InstancedWealth");
	if (HasActorWithTag(World, Tag)) return;

	// Spawn inside the Treasury building (Clearinghouse center, slightly offset)
	FActorSpawnParameters Params;
	Params.Name = Tag;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ATartariaInstancedWealth* Wealth = World->SpawnActor<ATartariaInstancedWealth>(
		ATartariaInstancedWealth::StaticClass(),
		FVector(0.f, 0.f, 10.f),  // Treasury location + slight Z offset
		FRotator::ZeroRotator, Params);

	if (Wealth)
	{
		Wealth->Tags.Add(Tag);
		Wealth->CurrentBars = 50;  // Starting wealth
		UE_LOG(LogTemp, Log, TEXT("[WorldPopulator] Spawned instanced wealth (50 gold bars, HISM)"));
	}
}

// -------------------------------------------------------
// Terrain — Ground planes per biome
// -------------------------------------------------------

void UTartariaWorldPopulator::SpawnTerrain(UWorld* World)
{
	const FName Tag("TerrainManager");
	if (HasActorWithTag(World, Tag)) return;

	FActorSpawnParameters Params;
	Params.Name = Tag;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ATartariaTerrainManager* Terrain = World->SpawnActor<ATartariaTerrainManager>(
		ATartariaTerrainManager::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);

	if (Terrain)
	{
		Terrain->Tags.Add(Tag);
		Terrain->SpawnTerrainPlanes(World);
	}
}

// -------------------------------------------------------
// Ambient Sounds (one per biome)
// -------------------------------------------------------

void UTartariaWorldPopulator::SpawnAmbientSounds(UWorld* World)
{
	struct FSoundDef
	{
		FName Tag;
		FString BiomeKey;
		FVector Location;
	};

	const TArray<FSoundDef> Sounds = {
		{ FName("AmbientSound_CLEARINGHOUSE"),  TEXT("CLEARINGHOUSE"),  FVector(0.f, 0.f, 500.f) },
		{ FName("AmbientSound_SCRIPTORIUM"),    TEXT("SCRIPTORIUM"),    FVector(250000.f, 0.f, 500.f) },
		{ FName("AmbientSound_MONOLITH_WARD"),  TEXT("MONOLITH_WARD"), FVector(0.f, 300000.f, 500.f) },
		{ FName("AmbientSound_FORGE_DISTRICT"), TEXT("FORGE_DISTRICT"), FVector(-250000.f, 0.f, 500.f) },
		{ FName("AmbientSound_VOID_REACH"),     TEXT("VOID_REACH"),     FVector(0.f, -350000.f, 500.f) },
	};

	int32 SpawnCount = 0;
	for (const FSoundDef& Def : Sounds)
	{
		if (HasActorWithTag(World, Def.Tag)) continue;

		FActorSpawnParameters Params;
		Params.Name = Def.Tag;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ATartariaAmbientSound* Sound = World->SpawnActor<ATartariaAmbientSound>(
			ATartariaAmbientSound::StaticClass(), Def.Location, FRotator::ZeroRotator, Params);

		if (Sound)
		{
			Sound->BiomeKey = Def.BiomeKey;
			Sound->Tags.Add(Def.Tag);
			Sound->ConfigureForBiome();
			++SpawnCount;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[WorldPopulator] Spawned %d ambient sound emitters"), SpawnCount);
}

// -------------------------------------------------------
// Biome Particles (one per biome, follows player)
// -------------------------------------------------------

void UTartariaWorldPopulator::SpawnBiomeParticles(UWorld* World)
{
	struct FParticleDef
	{
		FName Tag;
		FString BiomeKey;
		FVector Location;
	};

	const TArray<FParticleDef> Particles = {
		{ FName("Particles_CLEARINGHOUSE"),  TEXT("CLEARINGHOUSE"),  FVector(0.f, 0.f, 200.f) },
		{ FName("Particles_SCRIPTORIUM"),    TEXT("SCRIPTORIUM"),    FVector(250000.f, 0.f, 200.f) },
		{ FName("Particles_MONOLITH_WARD"),  TEXT("MONOLITH_WARD"), FVector(0.f, 300000.f, 200.f) },
		{ FName("Particles_FORGE_DISTRICT"), TEXT("FORGE_DISTRICT"), FVector(-250000.f, 0.f, 200.f) },
		{ FName("Particles_VOID_REACH"),     TEXT("VOID_REACH"),     FVector(0.f, -350000.f, 200.f) },
	};

	int32 SpawnCount = 0;
	for (const FParticleDef& Def : Particles)
	{
		if (HasActorWithTag(World, Def.Tag)) continue;

		FActorSpawnParameters Params;
		Params.Name = Def.Tag;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ATartariaBiomeParticles* Ptcl = World->SpawnActor<ATartariaBiomeParticles>(
			ATartariaBiomeParticles::StaticClass(), Def.Location, FRotator::ZeroRotator, Params);

		if (Ptcl)
		{
			Ptcl->BiomeKey = Def.BiomeKey;
			Ptcl->Tags.Add(Def.Tag);
			Ptcl->ConfigureForBiome();
			++SpawnCount;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[WorldPopulator] Spawned %d biome particle emitters"), SpawnCount);
}

// -------------------------------------------------------
// Patent Registry (holographic terminal near Scriptorium)
// -------------------------------------------------------

void UTartariaWorldPopulator::SpawnPatentRegistry(UWorld* World)
{
	const FName Tag("PatentRegistry");
	if (HasActorWithTag(World, Tag)) return;

	// Place near the Scriptorium entrance
	FActorSpawnParameters Params;
	Params.Name = Tag;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ATartariaPatentRegistry* Registry = World->SpawnActor<ATartariaPatentRegistry>(
		ATartariaPatentRegistry::StaticClass(),
		FVector(250200.f, 2000.f, 0.f),  // Just outside Scriptorium
		FRotator::ZeroRotator, Params);

	if (Registry)
	{
		Registry->Tags.Add(Tag);
		Registry->UpdatePatentCount(3);  // Start with 3 visible patents
		UE_LOG(LogTemp, Log, TEXT("[WorldPopulator] Spawned patent registry near Scriptorium"));
	}
}

// -------------------------------------------------------
// Alchemical Scales (trade balance indicator)
// -------------------------------------------------------

void UTartariaWorldPopulator::SpawnAlchemicalScales(UWorld* World)
{
	const FName Tag("AlchemicalScales");
	if (HasActorWithTag(World, Tag)) return;

	// Place in the Clearinghouse near the Treasury
	FActorSpawnParameters Params;
	Params.Name = Tag;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ATartariaAlchemicalScales* Scales = World->SpawnActor<ATartariaAlchemicalScales>(
		ATartariaAlchemicalScales::StaticClass(),
		FVector(500.f, 500.f, 0.f),  // Near Treasury in Clearinghouse
		FRotator::ZeroRotator, Params);

	if (Scales)
	{
		Scales->Tags.Add(Tag);
		Scales->UpdateTradeBalance(0.3f);  // Slightly prosperous by default
		UE_LOG(LogTemp, Log, TEXT("[WorldPopulator] Spawned alchemical scales"));
	}
}

// -------------------------------------------------------
// Faction Banners (on primary buildings, LOD by rep)
// -------------------------------------------------------

void UTartariaWorldPopulator::SpawnFactionBanners(UWorld* World)
{
	struct FBannerDef
	{
		FName Tag;
		FString FactionKey;
		float StartRep;
		FVector Location;
	};

	const TArray<FBannerDef> Banners = {
		// Clearinghouse — Stewards banner
		{ FName("Banner_Stewards"),    TEXT("ARGENTUM"),  45.f,  FVector(400.f, 0.f, 0.f) },
		// Forge District — Forge Masters
		{ FName("Banner_ForgeMasters"), TEXT("FERRUM"),    55.f,  FVector(-249600.f, 0.f, 0.f) },
		// Scriptorium — Archivists
		{ FName("Banner_Archivists"),  TEXT("AUREATE"),   40.f,  FVector(250400.f, 0.f, 0.f) },
		// Monolith Ward — Masons
		{ FName("Banner_Masons"),      TEXT("FERRUM"),    35.f,  FVector(400.f, 300000.f, 0.f) },
		// Void Reach — Void Walkers
		{ FName("Banner_VoidWalkers"), TEXT("OBSIDIAN"),  60.f,  FVector(400.f, -350000.f, 0.f) },
		// Secondary locations — outpost banners
		{ FName("Banner_Watchtower"),  TEXT("ARGENTUM"),  25.f,  FVector(80400.f, 40000.f, 0.f) },
		{ FName("Banner_Smelter"),     TEXT("FERRUM"),    50.f,  FVector(-199600.f, 60000.f, 0.f) },
		{ FName("Banner_Library"),     TEXT("AUREATE"),   30.f,  FVector(200400.f, 50000.f, 0.f) },
		{ FName("Banner_Quarry"),      TEXT("ARGENTUM"),  20.f,  FVector(50400.f, 250000.f, 0.f) },
		{ FName("Banner_Crystal"),     TEXT("OBSIDIAN"),  75.f,  FVector(40400.f, -300000.f, 0.f) },
	};

	int32 SpawnCount = 0;
	for (const FBannerDef& Def : Banners)
	{
		if (HasActorWithTag(World, Def.Tag)) continue;

		FActorSpawnParameters Params;
		Params.Name = Def.Tag;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ATartariaFactionBanner* Banner = World->SpawnActor<ATartariaFactionBanner>(
			ATartariaFactionBanner::StaticClass(),
			Def.Location, FRotator::ZeroRotator, Params);

		if (Banner)
		{
			Banner->Tags.Add(Def.Tag);
			Banner->SetFaction(Def.FactionKey, Def.StartRep);
			++SpawnCount;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[WorldPopulator] Spawned %d faction banners"), SpawnCount);
}

// -------------------------------------------------------
// Landmarks — biome navigation beacons (Task #208)
// -------------------------------------------------------

void UTartariaWorldPopulator::SpawnLandmarks(UWorld* World)
{
	struct FLandmarkDef
	{
		FName Tag;
		ETartariaBiome Biome;
		FVector Location;
		float Height;
	};

	// Place each landmark at the center of its biome, elevated slightly
	const TArray<FLandmarkDef> Landmarks = {
		{ FName("Landmark_CLEARINGHOUSE"),  ETartariaBiome::Clearinghouse,  FVector(0.f, 0.f, 0.f),          2000.f },
		{ FName("Landmark_SCRIPTORIUM"),    ETartariaBiome::Scriptorium,    FVector(250000.f, 0.f, 0.f),     2200.f },
		{ FName("Landmark_MONOLITH_WARD"),  ETartariaBiome::MonolithWard,   FVector(0.f, 300000.f, 0.f),     2500.f },
		{ FName("Landmark_FORGE_DISTRICT"), ETartariaBiome::ForgeDistrict,  FVector(-250000.f, 0.f, 0.f),    2300.f },
		{ FName("Landmark_VOID_REACH"),     ETartariaBiome::VoidReach,      FVector(0.f, -350000.f, 0.f),    2800.f },
	};

	int32 SpawnCount = 0;
	for (const FLandmarkDef& Def : Landmarks)
	{
		if (HasActorWithTag(World, Def.Tag)) continue;

		FActorSpawnParameters Params;
		Params.Name = Def.Tag;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ATartariaLandmark* Landmark = World->SpawnActor<ATartariaLandmark>(
			ATartariaLandmark::StaticClass(), Def.Location, FRotator::ZeroRotator, Params);

		if (Landmark)
		{
			Landmark->OwningBiome = Def.Biome;
			Landmark->LandmarkHeight = Def.Height;
			Landmark->Tags.Add(Def.Tag);

			// ConfigureForBiome() is called in BeginPlay, but since we set
			// OwningBiome after spawn, call it again explicitly
			Landmark->ConfigureForBiome();

			++SpawnCount;
			UE_LOG(LogTemp, Log, TEXT("[WorldPopulator] Spawned landmark for biome %d at %s (height=%.0f)"),
				static_cast<int32>(Def.Biome), *Def.Location.ToString(), Def.Height);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[WorldPopulator] Spawned %d biome landmarks"), SpawnCount);
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
