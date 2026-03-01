#include "TartariaLevelStreamManager.h"
#include "TartariaBaseProfile.h"
#include "TartariaDayNightCycle.h"
#include "TartariaCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

TWeakObjectPtr<ATartariaLevelStreamManager> ATartariaLevelStreamManager::Instance;

ATartariaLevelStreamManager::ATartariaLevelStreamManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ATartariaLevelStreamManager::BeginPlay()
{
	Super::BeginPlay();
	Instance = this;

	// Apply Earth's profile as the default state
	FTartariaBaseVisualProfile EarthProfile = UTartariaBaseProfile::GetProfile(TEXT("earth"));
	ApplyVisualProfile(EarthProfile);

	UE_LOG(LogTemp, Log, TEXT("TartariaLevelStreamManager: Initialized (body: earth)"));
}

ATartariaLevelStreamManager* ATartariaLevelStreamManager::Get(const UObject* WorldContextObject)
{
	if (Instance.IsValid()) return Instance.Get();

	if (!WorldContextObject) return nullptr;
	UWorld* World = WorldContextObject->GetWorld();
	if (!World) return nullptr;

	for (TActorIterator<ATartariaLevelStreamManager> It(World); It; ++It)
	{
		Instance = *It;
		return Instance.Get();
	}

	return nullptr;
}

// -------------------------------------------------------
// Tags used by WorldPopulator
// -------------------------------------------------------

const TArray<FName>& ATartariaLevelStreamManager::GetPopulatorTags()
{
	static TArray<FName> Tags = {
		// Biomes
		FName("Biome_CLEARINGHOUSE"), FName("Biome_SCRIPTORIUM"),
		FName("Biome_MONOLITH_WARD"), FName("Biome_FORGE_DISTRICT"),
		FName("Biome_VOID_REACH"),
		// Day/Night
		FName("DayNightCycle"),
		// Buildings
		FName("Building_Treasury"), FName("Building_Forge"),
		FName("Building_Scriptorium"), FName("Building_Barracks"),
		FName("Building_VoidLab"), FName("Building_Watchtower"),
		FName("Building_Smelter"), FName("Building_LibraryAnnex"),
		FName("Building_QuarryOffice"), FName("Building_CrystalSpire"),
		// Terminals
		FName("Terminal_Treasury"), FName("Terminal_Forge"),
		FName("Terminal_Scriptorium"), FName("Terminal_Barracks"),
		FName("Terminal_Lab"),
		// NPCs
		FName("NPC_TheSteward"), FName("NPC_TheArchivist"),
		FName("NPC_TheMason"), FName("NPC_TheComptroller"),
		FName("NPC_TheVoidWalker"), FName("NPC_TheAlchemist"),
		FName("NPC_TheForgeMaster"),
		// Resources (pattern: Resource_*)
		// We'll handle these via prefix matching instead
		// Quest markers
		FName("Quest_FirstDecree"), FName("Quest_GuildRegistration"),
		FName("Quest_LostManuscript"), FName("Quest_AncientArchive"),
		FName("Quest_MonolithMystery"), FName("Quest_StoneGiant"),
		FName("Quest_SmithChallenge"), FName("Quest_OreVein"),
		FName("Quest_VoidAnomaly"), FName("Quest_CrystalConverge"),
		// Ambient
		FName("AmbientSound_CLEARINGHOUSE"), FName("AmbientSound_SCRIPTORIUM"),
		FName("AmbientSound_MONOLITH_WARD"), FName("AmbientSound_FORGE_DISTRICT"),
		FName("AmbientSound_VOID_REACH"),
		// Particles
		FName("Particles_CLEARINGHOUSE"), FName("Particles_SCRIPTORIUM"),
		FName("Particles_MONOLITH_WARD"), FName("Particles_FORGE_DISTRICT"),
		FName("Particles_VOID_REACH"),
		// Terrain + Wealth
		FName("TerrainManager"), FName("InstancedWealth"),
		// Economy manifestation
		FName("PatentRegistry"), FName("AlchemicalScales"),
		FName("Banner_Stewards"), FName("Banner_ForgeMasters"),
		FName("Banner_Archivists"), FName("Banner_Masons"),
		FName("Banner_VoidWalkers"), FName("Banner_Watchtower"),
		FName("Banner_Smelter"), FName("Banner_Library"),
		FName("Banner_Quarry"), FName("Banner_Crystal"),
	};
	return Tags;
}

// -------------------------------------------------------
// Transition API
// -------------------------------------------------------

void ATartariaLevelStreamManager::TransitionToBody(const FString& NewBodyName)
{
	if (bTransitioning)
	{
		UE_LOG(LogTemp, Warning, TEXT("LevelStreamManager: Transition already in progress"));
		return;
	}

	if (NewBodyName.Equals(CurrentLoadedBody, ESearchCase::IgnoreCase))
	{
		UE_LOG(LogTemp, Log, TEXT("LevelStreamManager: Already at %s"), *NewBodyName);
		return;
	}

	bTransitioning = true;
	PendingBody = NewBodyName;

	UE_LOG(LogTemp, Log, TEXT("LevelStreamManager: Transitioning %s -> %s"),
		*CurrentLoadedBody, *NewBodyName);

	// Fade camera to black
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC)
	{
		PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, 1.0f, FLinearColor::Black, false, true);
	}

	// After 1.0s fade-out, do the actual content swap
	GetWorldTimerManager().SetTimer(
		FadeOutTimerHandle, this,
		&ATartariaLevelStreamManager::OnFadeOutComplete,
		1.0f, false);
}

void ATartariaLevelStreamManager::OnFadeOutComplete()
{
	bool bReturningToEarth = PendingBody.Equals(TEXT("earth"), ESearchCase::IgnoreCase);

	if (bReturningToEarth)
	{
		// Returning to Earth: destroy temp terrain, re-show original actors
		DestroyBodyTerrain();
		ShowWorldPopulatorActors();
	}
	else
	{
		// Leaving Earth (or another body): hide Earth actors, spawn temp terrain
		if (CurrentLoadedBody.Equals(TEXT("earth"), ESearchCase::IgnoreCase))
		{
			HideWorldPopulatorActors();
		}
		else
		{
			// Going from one non-Earth body to another: just replace terrain
			DestroyBodyTerrain();
		}
	}

	// Get and apply the new body's visual profile
	FTartariaBaseVisualProfile Profile = UTartariaBaseProfile::GetProfile(PendingBody);
	ApplyVisualProfile(Profile);

	if (!bReturningToEarth)
	{
		SpawnBodyTerrain(Profile);
	}

	// Update gravity on player character
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (ATartariaCharacter* PlayerChar = Cast<ATartariaCharacter>(PlayerPawn))
	{
		PlayerChar->GetCharacterMovement()->GravityScale = Profile.GravityMultiplier;

		// Adjust jump velocity for low-gravity bodies (higher jumps)
		float JumpMul = (Profile.GravityMultiplier > 0.01f)
			? FMath::Sqrt(1.0f / Profile.GravityMultiplier)
			: 3.0f;
		PlayerChar->GetCharacterMovement()->JumpZVelocity = 600.f * FMath::Min(JumpMul, 3.0f);
	}

	// Update current body
	FString OldBody = CurrentLoadedBody;
	CurrentLoadedBody = PendingBody;

	UE_LOG(LogTemp, Log, TEXT("LevelStreamManager: Content swapped %s -> %s"), *OldBody, *CurrentLoadedBody);

	// Fade back in after a short delay (let things settle)
	GetWorldTimerManager().SetTimer(
		FadeInTimerHandle, this,
		&ATartariaLevelStreamManager::FadeIn,
		0.3f, false);
}

void ATartariaLevelStreamManager::FadeIn()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC)
	{
		PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 1.5f, FLinearColor::Black, false, false);
	}

	bTransitioning = false;
	OnBodyLoaded.Broadcast(CurrentLoadedBody);

	UE_LOG(LogTemp, Log, TEXT("LevelStreamManager: Transition complete, now at %s"), *CurrentLoadedBody);
}

// -------------------------------------------------------
// Visual Profile Application
// -------------------------------------------------------

void ATartariaLevelStreamManager::ApplyVisualProfile(const FTartariaBaseVisualProfile& Profile)
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Find and update the DayNightCycle actor
	for (TActorIterator<ATartariaDayNightCycle> It(World); It; ++It)
	{
		ATartariaDayNightCycle* DNC = *It;
		if (DNC && !DNC->IsHidden())
		{
			DNC->DayLengthSeconds = (Profile.DayCycleSpeed > 0.001f)
				? 600.f / Profile.DayCycleSpeed  // Base 10-min day scaled
				: 0.f;  // No day/night cycle

			// Update sun properties if the DNC has a directional light
			// (The DNC manages its own sun rotation, so we just adjust color/intensity)
		}
	}

	// Find directional light (sun) and update
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		UDirectionalLightComponent* DirLight = It->FindComponentByClass<UDirectionalLightComponent>();
		if (DirLight)
		{
			DirLight->SetLightColor(Profile.SunColor);
			DirLight->SetIntensity(Profile.SunIntensity);
			break;  // Only update the primary sun
		}
	}

	// Find sky light and update
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		USkyLightComponent* SkyLight = It->FindComponentByClass<USkyLightComponent>();
		if (SkyLight)
		{
			SkyLight->SetLightColor(Profile.AmbientColor);
			SkyLight->SetIntensity(Profile.AmbientIntensity);
			break;
		}
	}

	// Find exponential height fog and update
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		UExponentialHeightFogComponent* Fog = It->FindComponentByClass<UExponentialHeightFogComponent>();
		if (Fog)
		{
			Fog->SetFogDensity(Profile.FogDensity);
			Fog->SetFogInscatteringColor(Profile.FogColor);
			break;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("LevelStreamManager: Applied visual profile for %s (gravity=%.3f, dayCycle=%.3f)"),
		*Profile.BodyName, Profile.GravityMultiplier, Profile.DayCycleSpeed);
}

// -------------------------------------------------------
// Actor Show/Hide
// -------------------------------------------------------

void ATartariaLevelStreamManager::HideWorldPopulatorActors()
{
	UWorld* World = GetWorld();
	if (!World) return;

	HiddenActors.Empty();

	const TArray<FName>& Tags = GetPopulatorTags();

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor == this) continue;

		bool bShouldHide = false;

		// Check explicit tags
		for (const FName& Tag : Tags)
		{
			if (Actor->Tags.Contains(Tag))
			{
				bShouldHide = true;
				break;
			}
		}

		// Also check Resource_ prefix tags
		if (!bShouldHide)
		{
			for (const FName& ActorTag : Actor->Tags)
			{
				if (ActorTag.ToString().StartsWith(TEXT("Resource_")))
				{
					bShouldHide = true;
					break;
				}
			}
		}

		if (bShouldHide)
		{
			Actor->SetActorHiddenInGame(true);
			Actor->SetActorEnableCollision(false);
			Actor->SetActorTickEnabled(false);
			HiddenActors.Add(Actor);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("LevelStreamManager: Hidden %d world actors"), HiddenActors.Num());
}

void ATartariaLevelStreamManager::ShowWorldPopulatorActors()
{
	for (AActor* Actor : HiddenActors)
	{
		if (IsValid(Actor))
		{
			Actor->SetActorHiddenInGame(false);
			Actor->SetActorEnableCollision(true);
			Actor->SetActorTickEnabled(true);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("LevelStreamManager: Restored %d world actors"), HiddenActors.Num());
	HiddenActors.Empty();
}

// -------------------------------------------------------
// Temporary Body Terrain
// -------------------------------------------------------

void ATartariaLevelStreamManager::SpawnBodyTerrain(const FTartariaBaseVisualProfile& Profile)
{
	UWorld* World = GetWorld();
	if (!World) return;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	BodyTerrainActor = World->SpawnActor<AActor>(AActor::StaticClass(),
		FVector(0.f, 0.f, -10.f), FRotator::ZeroRotator, Params);

	if (!BodyTerrainActor) return;

	// Create root component
	USceneComponent* Root = NewObject<USceneComponent>(BodyTerrainActor, TEXT("Root"));
	BodyTerrainActor->SetRootComponent(Root);
	Root->RegisterComponent();

	// Ground plane — large flat plane with body's ground color
	UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (!PlaneMesh) return;

	UStaticMeshComponent* Ground = NewObject<UStaticMeshComponent>(BodyTerrainActor, TEXT("BodyGround"));
	Ground->SetupAttachment(Root);
	Ground->SetStaticMesh(PlaneMesh);
	Ground->SetRelativeScale3D(FVector(3000.f, 3000.f, 1.f));  // 300km diameter
	Ground->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Ground->SetCollisionResponseToAllChannels(ECR_Block);
	Ground->SetCollisionObjectType(ECC_WorldStatic);
	Ground->RegisterComponent();

	// Apply ground color
	UMaterialInstanceDynamic* GroundMat = Ground->CreateAndSetMaterialInstanceDynamic(0);
	if (GroundMat)
	{
		GroundMat->SetVectorParameterValue(TEXT("Color"), Profile.GroundColor);
	}

	// Add a subtle sky-colored ambient light at ground level
	UPointLightComponent* AmbientGlow = NewObject<UPointLightComponent>(BodyTerrainActor, TEXT("AmbientGlow"));
	AmbientGlow->SetupAttachment(Root);
	AmbientGlow->SetRelativeLocation(FVector(0.f, 0.f, 5000.f));
	AmbientGlow->SetLightColor(Profile.AmbientColor);
	AmbientGlow->SetIntensity(Profile.AmbientIntensity * 5000.f);
	AmbientGlow->SetAttenuationRadius(500000.f);
	AmbientGlow->CastShadows = false;
	AmbientGlow->RegisterComponent();

	UE_LOG(LogTemp, Log, TEXT("LevelStreamManager: Spawned terrain for %s"), *Profile.BodyName);
}

void ATartariaLevelStreamManager::DestroyBodyTerrain()
{
	if (BodyTerrainActor && IsValid(BodyTerrainActor))
	{
		BodyTerrainActor->Destroy();
		BodyTerrainActor = nullptr;
		UE_LOG(LogTemp, Log, TEXT("LevelStreamManager: Destroyed temporary body terrain"));
	}
}
