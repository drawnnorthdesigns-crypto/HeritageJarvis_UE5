#include "TartariaNPC.h"
#include "Core/HJGameInstance.h"
#include "Core/HJApiClient.h"
#include "Core/TartariaSoundManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Async/Async.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"

ATartariaNPC::ATartariaNPC()
{
	PrimaryActorTick.bCanEverTick = true;

	// ── Load engine primitives (constructor-only) ──────────────
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder"));

	UStaticMesh* CubeMesh   = CubeFinder.Succeeded()     ? CubeFinder.Object     : nullptr;
	UStaticMesh* SphereMesh  = SphereFinder.Succeeded()   ? SphereFinder.Object   : nullptr;
	UStaticMesh* CylinderMesh = CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr;

	// Default neutral grey — specialist colors applied in BeginPlay
	FLinearColor DefaultColor(0.5f, 0.5f, 0.5f);

	// ── Torso (box) ───────────────────────────────────────────
	BodyTorso = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyTorso"));
	BodyTorso->SetupAttachment(RootComponent);
	if (CubeMesh) BodyTorso->SetStaticMesh(CubeMesh);
	BodyTorso->SetRelativeLocation(FVector(0.f, 0.f, 10.f));
	BodyTorso->SetRelativeScale3D(FVector(0.4f, 0.25f, 0.5f));
	BodyTorso->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// ── Head (sphere) ─────────────────────────────────────────
	BodyHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyHead"));
	BodyHead->SetupAttachment(BodyTorso);
	if (SphereMesh) BodyHead->SetStaticMesh(SphereMesh);
	BodyHead->SetRelativeLocation(FVector(0.f, 0.f, 65.f));
	BodyHead->SetRelativeScale3D(FVector(0.3f, 0.3f, 0.32f));
	BodyHead->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// ── Left Arm (cylinder) ───────────────────────────────────
	BodyArmL = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyArmL"));
	BodyArmL->SetupAttachment(BodyTorso);
	if (CylinderMesh) BodyArmL->SetStaticMesh(CylinderMesh);
	BodyArmL->SetRelativeLocation(FVector(0.f, -30.f, 20.f));
	BodyArmL->SetRelativeScale3D(FVector(0.1f, 0.1f, 0.4f));
	BodyArmL->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// ── Right Arm (cylinder) ──────────────────────────────────
	BodyArmR = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyArmR"));
	BodyArmR->SetupAttachment(BodyTorso);
	if (CylinderMesh) BodyArmR->SetStaticMesh(CylinderMesh);
	BodyArmR->SetRelativeLocation(FVector(0.f, 30.f, 20.f));
	BodyArmR->SetRelativeScale3D(FVector(0.1f, 0.1f, 0.4f));
	BodyArmR->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// ── Left Leg (cylinder) ───────────────────────────────────
	BodyLegL = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyLegL"));
	BodyLegL->SetupAttachment(RootComponent);
	if (CylinderMesh) BodyLegL->SetStaticMesh(CylinderMesh);
	BodyLegL->SetRelativeLocation(FVector(0.f, -12.f, -45.f));
	BodyLegL->SetRelativeScale3D(FVector(0.12f, 0.12f, 0.45f));
	BodyLegL->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// ── Right Leg (cylinder) ──────────────────────────────────
	BodyLegR = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyLegR"));
	BodyLegR->SetupAttachment(RootComponent);
	if (CylinderMesh) BodyLegR->SetStaticMesh(CylinderMesh);
	BodyLegR->SetRelativeLocation(FVector(0.f, 12.f, -45.f));
	BodyLegR->SetRelativeScale3D(FVector(0.12f, 0.12f, 0.45f));
	BodyLegR->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Accessory is spawned at runtime in BeginPlay (needs SpecialistType from editor)
	Accessory = nullptr;

	// ── Floating Name Tag ─────────────────────────────────────
	NameTag = CreateDefaultSubobject<UTextRenderComponent>(TEXT("NameTag"));
	NameTag->SetupAttachment(RootComponent);
	NameTag->SetRelativeLocation(FVector(0.f, 0.f, 110.f));  // Above head
	NameTag->SetHorizontalAlignment(EHTA_Center);
	NameTag->SetVerticalAlignment(EVRTA_TextCenter);
	NameTag->SetWorldSize(18.f);
	NameTag->SetTextRenderColor(FColor(200, 180, 120));  // Default warm gold
	NameTag->SetText(FText::FromString(NPCName));

	// ── Mood Indicator Light ──────────────────────────────────
	MoodLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MoodLight"));
	MoodLight->SetupAttachment(RootComponent);
	MoodLight->SetRelativeLocation(FVector(0.f, 0.f, 130.f));  // Above name tag
	MoodLight->SetIntensity(500.f);
	MoodLight->SetAttenuationRadius(80.f);
	MoodLight->SetLightColor(FLinearColor(0.7f, 0.7f, 0.5f));  // Neutral warm
	MoodLight->SetCastShadows(false);
}

void ATartariaNPC::BeginPlay()
{
	Super::BeginPlay();

	// Apply specialist-specific colors and spawn accessory now that
	// SpecialistType has been set by the editor or WorldPopulator.
	ApplySpecialistAppearance();

	// Spawn environmental workshop props around the NPC
	SpawnWorkshopProps();
}

void ATartariaNPC::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ── Breathing Animation (torso scale oscillation) ────────
	BreathPhase += DeltaTime * 1.5f;
	if (BodyTorso)
	{
		float BreathScale = 1.0f + FMath::Sin(BreathPhase) * 0.008f;  // 0.8% oscillation
		FVector BaseScale(0.4f, 0.25f, 0.5f);
		BodyTorso->SetRelativeScale3D(BaseScale * FVector(1.f, 1.f, BreathScale));
	}

	// ── Idle Arm Sway ────────────────────────────────────────
	if (BodyArmL)
	{
		float ArmSway = FMath::Sin(BreathPhase * 0.6f) * 5.0f;  // 5 degree gentle sway
		BodyArmL->SetRelativeRotation(FRotator(ArmSway, 0.f, 0.f));
	}
	// Note: BodyArmR idle sway is overridden below when player is near
	bool bPlayerNearby = false;

	// ── Head tracking toward nearest player ──────────────────
	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(this, 0);
	if (Player && BodyHead)
	{
		float DistToPlayer = FVector::Dist(GetActorLocation(), Player->GetActorLocation());
		if (DistToPlayer < 500.f)  // Within interaction range
		{
			bPlayerNearby = true;

			// Turn body toward player
			FVector ToPlayer = (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			float LookYaw = FMath::Atan2(ToPlayer.Y, ToPlayer.X) * (180.f / PI);
			FRotator CurrentRot = GetActorRotation();
			FRotator TargetRot = FRotator(0.f, LookYaw, 0.f);
			SetActorRotation(FMath::RInterpTo(CurrentRot, TargetRot, DeltaTime, 3.f));

			// Head tilt toward player (subtle pitch)
			float HeadPitch = FMath::Clamp((DistToPlayer - 200.f) * -0.02f, -8.f, 0.f);
			BodyHead->SetRelativeRotation(FRotator(HeadPitch, 0.f, 0.f));

			// Greeting gesture: raise right arm when player approaches
			if (DistToPlayer < 300.f && BodyArmR)
			{
				float GestureAngle = FMath::FInterpTo(
					BodyArmR->GetRelativeRotation().Pitch, -30.f, DeltaTime, 4.f);
				BodyArmR->SetRelativeRotation(FRotator(GestureAngle, 0.f, 15.f));
			}
		}
		else
		{
			// Reset head when player is far
			FRotator HeadRot = BodyHead->GetRelativeRotation();
			BodyHead->SetRelativeRotation(FMath::RInterpTo(HeadRot, FRotator::ZeroRotator, DeltaTime, 2.f));
		}
	}

	// Right arm idle sway (only when not gesturing at player and not playing action gesture)
	if (!bPlayerNearby && !bPlayingGesture && BodyArmR)
	{
		float ArmSway = FMath::Sin(BreathPhase * 0.6f + PI) * 5.0f;  // opposite phase
		BodyArmR->SetRelativeRotation(FRotator(ArmSway, 0.f, 0.f));
	}

	// ── Action Gesture Animation ────────────────────────────
	if (bPlayingGesture)
	{
		ActionGestureTimer += DeltaTime;
		float Phase = ActionGestureTimer / ActionGestureDuration;

		if (CurrentGestureType.Contains(TEXT("forge")) || CurrentGestureType.Contains(TEXT("craft")))
		{
			// Hammering motion — arm pumps up and down
			if (BodyArmR)
			{
				float HammerAngle = FMath::Sin(Phase * PI * 6.f) * 40.f - 20.f;
				BodyArmR->SetRelativeRotation(FRotator(HammerAngle, 0.f, 10.f));
			}
			// Lean forward while working
			if (BodyTorso)
			{
				BodyTorso->SetRelativeRotation(FRotator(8.f, 0.f, 0.f));
			}
		}
		else if (CurrentGestureType.Contains(TEXT("read")) || CurrentGestureType.Contains(TEXT("research")))
		{
			// Reading — both arms forward, head dipped
			if (BodyArmL)
				BodyArmL->SetRelativeRotation(FRotator(-30.f, 0.f, -15.f));
			if (BodyArmR)
				BodyArmR->SetRelativeRotation(FRotator(-30.f, 0.f, 15.f));
			if (BodyHead)
				BodyHead->SetRelativeRotation(FRotator(-10.f, 0.f, 0.f));
		}
		else if (CurrentGestureType.Contains(TEXT("analyze")) || CurrentGestureType.Contains(TEXT("scan")))
		{
			// Analyzing — one arm raised with instrument, head tilted
			if (BodyArmR)
			{
				float ScanAngle = FMath::Sin(Phase * PI * 2.f) * 20.f;
				BodyArmR->SetRelativeRotation(FRotator(-50.f + ScanAngle, 15.f, 20.f));
			}
			if (BodyHead)
			{
				float HeadTilt = FMath::Sin(Phase * PI * 1.5f) * 5.f;
				BodyHead->SetRelativeRotation(FRotator(-5.f, HeadTilt, 0.f));
			}
		}
		else if (CurrentGestureType.Contains(TEXT("brew")) || CurrentGestureType.Contains(TEXT("mix")))
		{
			// Brewing — stirring motion with right arm
			if (BodyArmR)
			{
				float StirX = FMath::Sin(Phase * PI * 4.f) * 15.f;
				float StirY = FMath::Cos(Phase * PI * 4.f) * 15.f;
				BodyArmR->SetRelativeRotation(FRotator(-35.f + StirX, StirY, 10.f));
			}
			if (BodyTorso)
			{
				BodyTorso->SetRelativeRotation(FRotator(5.f, 0.f, 0.f));
			}
		}
		else
		{
			// Generic gesture — raise both arms briefly
			if (BodyArmL)
				BodyArmL->SetRelativeRotation(FRotator(-25.f, 0.f, -20.f));
			if (BodyArmR)
				BodyArmR->SetRelativeRotation(FRotator(-25.f, 0.f, 20.f));
		}

		if (ActionGestureTimer >= ActionGestureDuration)
		{
			bPlayingGesture = false;
			// Reset to idle poses
			if (BodyTorso)
				BodyTorso->SetRelativeRotation(FRotator::ZeroRotator);
			if (BodyHead)
				BodyHead->SetRelativeRotation(FRotator::ZeroRotator);
		}
	}

	// ── Idle Variation System ──────────────────────────────
	if (!bPlayingGesture && !bPlayerNearby)
	{
		IdleVariationTimer += DeltaTime;
		if (IdleVariationTimer >= NextIdleSwitchTime)
		{
			IdleVariationTimer = 0.f;
			CurrentIdleVariant = FMath::RandRange(0, 3);
			NextIdleSwitchTime = FMath::RandRange(12.f, 30.f);
		}

		switch (CurrentIdleVariant)
		{
		case 1:  // Look around — head slowly pans left/right
			if (BodyHead)
			{
				float LookYaw = FMath::Sin(BreathPhase * 0.3f) * 25.f;
				BodyHead->SetRelativeRotation(FRotator(0.f, LookYaw, 0.f));
			}
			break;

		case 2:  // Inspect tool — right arm raised, examining accessory
			if (BodyArmR)
			{
				float InspectAngle = -35.f + FMath::Sin(BreathPhase * 0.5f) * 5.f;
				BodyArmR->SetRelativeRotation(FRotator(InspectAngle, 10.f, 15.f));
			}
			if (BodyHead)
			{
				BodyHead->SetRelativeRotation(FRotator(-8.f, 12.f, 0.f));
			}
			break;

		case 3:  // Shift weight — torso sways slightly side to side
			if (BodyTorso)
			{
				float SwayRoll = FMath::Sin(BreathPhase * 0.4f) * 3.f;
				FVector BaseScale(0.4f, 0.25f, 0.5f);
				float BreathScale2 = 1.0f + FMath::Sin(BreathPhase) * 0.008f;
				BodyTorso->SetRelativeScale3D(BaseScale * FVector(1.f, 1.f, BreathScale2));
				BodyTorso->SetRelativeRotation(FRotator(0.f, 0.f, SwayRoll));
			}
			break;

		default:  // Variant 0: default idle (breathing + arm sway already handled above)
			break;
		}
	}
}

// -------------------------------------------------------
// Specialist Appearance — colors + accessory
// -------------------------------------------------------

void ATartariaNPC::ApplyColorToMesh(UStaticMeshComponent* Mesh, const FLinearColor& Color)
{
	if (!Mesh) return;
	UMaterialInstanceDynamic* DynMat = Mesh->CreateAndSetMaterialInstanceDynamic(0);
	if (DynMat)
	{
		DynMat->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

void ATartariaNPC::ApplySpecialistAppearance()
{
	// ── Color scheme per specialist type ──────────────────────
	FLinearColor SkinColor;
	FLinearColor AccentColor;
	const TCHAR* AccessoryShapePath = nullptr;
	FVector AccessoryLoc = FVector::ZeroVector;
	FVector AccessoryScale = FVector(1.f);

	switch (SpecialistType)
	{
	case ETartariaSpecialist::ForgeMaster:
		SkinColor   = FLinearColor(0.7f, 0.4f, 0.2f);   // warm bronze
		AccentColor = FLinearColor(0.9f, 0.5f, 0.1f);    // orange glow
		AccessoryShapePath = TEXT("/Engine/BasicShapes/Cylinder");  // hammer handle
		AccessoryLoc   = FVector(30.f, 0.f, -20.f);
		AccessoryScale = FVector(0.08f, 0.08f, 0.4f);
		break;
	case ETartariaSpecialist::Scribe:
		SkinColor   = FLinearColor(0.3f, 0.4f, 0.6f);   // ink blue
		AccentColor = FLinearColor(0.8f, 0.75f, 0.5f);   // parchment gold
		AccessoryShapePath = TEXT("/Engine/BasicShapes/Cube");  // tome
		AccessoryLoc   = FVector(25.f, 0.f, -10.f);
		AccessoryScale = FVector(0.2f, 0.15f, 0.25f);
		break;
	case ETartariaSpecialist::Alchemist:
		SkinColor   = FLinearColor(0.3f, 0.6f, 0.3f);   // verdant green
		AccentColor = FLinearColor(0.4f, 0.9f, 0.2f);    // acid green
		AccessoryShapePath = TEXT("/Engine/BasicShapes/Sphere");  // flask
		AccessoryLoc   = FVector(25.f, 0.f, -15.f);
		AccessoryScale = FVector(0.15f, 0.15f, 0.2f);
		break;
	case ETartariaSpecialist::WarCaptain:
		SkinColor   = FLinearColor(0.6f, 0.2f, 0.2f);   // war red
		AccentColor = FLinearColor(0.8f, 0.8f, 0.3f);    // gold trim
		AccessoryShapePath = TEXT("/Engine/BasicShapes/Cube");  // tactical slate
		AccessoryLoc   = FVector(28.f, 0.f, -5.f);
		AccessoryScale = FVector(0.25f, 0.02f, 0.18f);
		break;
	case ETartariaSpecialist::Surveyor:
		SkinColor   = FLinearColor(0.5f, 0.45f, 0.35f);  // earthy tan
		AccentColor = FLinearColor(0.6f, 0.8f, 0.9f);    // sky blue
		AccessoryShapePath = TEXT("/Engine/BasicShapes/Cylinder");  // spyglass
		AccessoryLoc   = FVector(30.f, 0.f, -8.f);
		AccessoryScale = FVector(0.05f, 0.05f, 0.35f);
		break;
	case ETartariaSpecialist::Governor:
		SkinColor   = FLinearColor(0.5f, 0.3f, 0.6f);   // regal purple
		AccentColor = FLinearColor(0.85f, 0.7f, 0.3f);   // sovereign gold
		AccessoryShapePath = TEXT("/Engine/BasicShapes/Cylinder");  // scepter
		AccessoryLoc   = FVector(25.f, 0.f, -25.f);
		AccessoryScale = FVector(0.04f, 0.04f, 0.5f);
		break;
	default: // Steward
		SkinColor   = FLinearColor(0.5f, 0.5f, 0.5f);   // neutral grey
		AccentColor = FLinearColor(0.7f, 0.6f, 0.4f);    // warm accent
		AccessoryShapePath = TEXT("/Engine/BasicShapes/Cube");  // ledger
		AccessoryLoc   = FVector(25.f, 0.f, -10.f);
		AccessoryScale = FVector(0.2f, 0.12f, 0.28f);
		break;
	}

	// ── Apply colors to body parts ───────────────────────────
	ApplyColorToMesh(BodyTorso, SkinColor);
	ApplyColorToMesh(BodyHead,  SkinColor * 1.1f);  // slightly lighter
	ApplyColorToMesh(BodyArmL,  SkinColor * 0.9f);
	ApplyColorToMesh(BodyArmR,  SkinColor * 0.9f);
	ApplyColorToMesh(BodyLegL,  SkinColor * 0.8f);
	ApplyColorToMesh(BodyLegR,  SkinColor * 0.8f);

	// ── Destroy old accessory if re-applying (WorldPopulator calls this after BeginPlay) ──
	if (Accessory)
	{
		Accessory->DestroyComponent();
		Accessory = nullptr;
	}

	// ── Spawn accessory at runtime ───────────────────────────
	if (AccessoryShapePath)
	{
		UStaticMesh* AccMesh = LoadObject<UStaticMesh>(nullptr, AccessoryShapePath);
		if (AccMesh && BodyArmR)
		{
			Accessory = NewObject<UStaticMeshComponent>(this, TEXT("Accessory"));
			Accessory->SetStaticMesh(AccMesh);
			Accessory->SetRelativeLocation(AccessoryLoc);
			Accessory->SetRelativeScale3D(AccessoryScale);
			Accessory->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Accessory->AttachToComponent(BodyArmR, FAttachmentTransformRules::KeepRelativeTransform);
			Accessory->RegisterComponent();
			ApplyColorToMesh(Accessory, AccentColor);
		}
	}

	// ── Update floating name tag ────────────────────────────
	if (NameTag)
	{
		// Build display name from specialist profile
		FString DisplayTitle;
		switch (SpecialistType)
		{
		case ETartariaSpecialist::ForgeMaster: DisplayTitle = TEXT("Forge Master Volkan"); break;
		case ETartariaSpecialist::Scribe:      DisplayTitle = TEXT("Scribe Aurelius"); break;
		case ETartariaSpecialist::Alchemist:   DisplayTitle = TEXT("Alchemist Viridian"); break;
		case ETartariaSpecialist::WarCaptain:  DisplayTitle = TEXT("War Captain Kratos"); break;
		case ETartariaSpecialist::Surveyor:    DisplayTitle = TEXT("Surveyor Meridian"); break;
		case ETartariaSpecialist::Governor:    DisplayTitle = TEXT("Governor Theron"); break;
		default:                                DisplayTitle = TEXT("Steward Cassius"); break;
		}
		NPCName = DisplayTitle;  // Update NPCName for dialogue
		NameTag->SetText(FText::FromString(DisplayTitle));

		// Color the name tag to match specialist accent
		FColor TagColor(
			FMath::Clamp(int32(AccentColor.R * 255), 0, 255),
			FMath::Clamp(int32(AccentColor.G * 255), 0, 255),
			FMath::Clamp(int32(AccentColor.B * 255), 0, 255)
		);
		NameTag->SetTextRenderColor(TagColor);
	}
}

// -------------------------------------------------------
// Workshop Props — environmental dressing per specialist
// -------------------------------------------------------

void ATartariaNPC::SpawnWorkshopProps()
{
	UWorld* World = GetWorld();
	if (!World) return;

	FVector Base = GetActorLocation();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// Helper lambda to spawn a primitive prop
	auto SpawnProp = [&](const TCHAR* ShapePath, const FVector& Offset, const FVector& Scale, const FLinearColor& Color) -> AActor*
	{
		AActor* PropActor = World->SpawnActor<AActor>(AActor::StaticClass(), Base + Offset, FRotator::ZeroRotator, Params);
		if (!PropActor) return nullptr;

		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(PropActor);
		Mesh->RegisterComponent();
		PropActor->SetRootComponent(Mesh);

		UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, ShapePath);
		if (SM) Mesh->SetStaticMesh(SM);

		Mesh->SetWorldScale3D(Scale);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		UMaterialInstanceDynamic* DynMat = Mesh->CreateAndSetMaterialInstanceDynamic(0);
		if (DynMat) DynMat->SetVectorParameterValue(TEXT("Color"), Color);

		WorkshopProps.Add(PropActor);
		return PropActor;
	};

	// Helper lambda to spawn a point light for ambiance
	auto SpawnPropLight = [&](const FVector& Offset, const FLinearColor& Color, float Intensity, float Radius)
	{
		AActor* LightActor = World->SpawnActor<AActor>(AActor::StaticClass(), Base + Offset, FRotator::ZeroRotator, Params);
		if (!LightActor) return;

		UPointLightComponent* Light = NewObject<UPointLightComponent>(LightActor);
		Light->RegisterComponent();
		LightActor->SetRootComponent(Light);
		Light->SetIntensity(Intensity);
		Light->SetLightColor(Color);
		Light->SetAttenuationRadius(Radius);
		WorkshopProps.Add(LightActor);
	};

	switch (SpecialistType)
	{
	case ETartariaSpecialist::ForgeMaster:
		// Anvil (flat cube), forge pit (cylinder with orange glow), bellows (box)
		SpawnProp(TEXT("/Engine/BasicShapes/Cube"), FVector(150.f, -60.f, -40.f), FVector(0.6f, 0.4f, 0.2f), FLinearColor(0.3f, 0.3f, 0.35f));  // anvil
		SpawnProp(TEXT("/Engine/BasicShapes/Cylinder"), FVector(120.f, 60.f, -50.f), FVector(0.5f, 0.5f, 0.15f), FLinearColor(0.15f, 0.08f, 0.05f));  // forge pit
		SpawnProp(TEXT("/Engine/BasicShapes/Cube"), FVector(180.f, 20.f, -35.f), FVector(0.2f, 0.15f, 0.25f), FLinearColor(0.4f, 0.25f, 0.15f));  // bellows
		SpawnPropLight(FVector(120.f, 60.f, -20.f), FLinearColor(1.0f, 0.5f, 0.1f), 5000.f, 300.f);  // forge fire glow
		break;

	case ETartariaSpecialist::Scribe:
		// Desk (flat cube), book stack (tall narrow cube), inkwell (small sphere)
		SpawnProp(TEXT("/Engine/BasicShapes/Cube"), FVector(140.f, 0.f, -30.f), FVector(0.7f, 0.4f, 0.04f), FLinearColor(0.45f, 0.3f, 0.15f));  // desk
		SpawnProp(TEXT("/Engine/BasicShapes/Cube"), FVector(140.f, -25.f, -10.f), FVector(0.15f, 0.1f, 0.3f), FLinearColor(0.5f, 0.35f, 0.2f));  // book stack
		SpawnProp(TEXT("/Engine/BasicShapes/Sphere"), FVector(140.f, 25.f, -22.f), FVector(0.05f, 0.05f, 0.06f), FLinearColor(0.05f, 0.05f, 0.15f));  // inkwell
		SpawnPropLight(FVector(140.f, 0.f, 20.f), FLinearColor(0.9f, 0.85f, 0.6f), 2000.f, 250.f);  // candle glow
		break;

	case ETartariaSpecialist::Alchemist:
		// Table (flat cube), 3 bottles (small spheres), cauldron (cylinder + green glow)
		SpawnProp(TEXT("/Engine/BasicShapes/Cube"), FVector(130.f, 0.f, -30.f), FVector(0.6f, 0.35f, 0.04f), FLinearColor(0.35f, 0.3f, 0.25f));  // table
		SpawnProp(TEXT("/Engine/BasicShapes/Sphere"), FVector(130.f, -20.f, -18.f), FVector(0.06f, 0.06f, 0.08f), FLinearColor(0.2f, 0.6f, 0.2f));  // green bottle
		SpawnProp(TEXT("/Engine/BasicShapes/Sphere"), FVector(130.f, 0.f, -18.f), FVector(0.05f, 0.05f, 0.1f), FLinearColor(0.6f, 0.2f, 0.5f));  // purple bottle
		SpawnProp(TEXT("/Engine/BasicShapes/Sphere"), FVector(130.f, 20.f, -18.f), FVector(0.07f, 0.07f, 0.07f), FLinearColor(0.8f, 0.7f, 0.1f));  // amber bottle
		SpawnProp(TEXT("/Engine/BasicShapes/Cylinder"), FVector(160.f, 50.f, -45.f), FVector(0.35f, 0.35f, 0.2f), FLinearColor(0.1f, 0.1f, 0.1f));  // cauldron
		SpawnPropLight(FVector(160.f, 50.f, -20.f), FLinearColor(0.1f, 0.9f, 0.2f), 3000.f, 250.f);  // cauldron green glow
		break;

	case ETartariaSpecialist::WarCaptain:
		// Tactical table (cube), map (flat plane-like cube), weapon rack (tall narrow cube)
		SpawnProp(TEXT("/Engine/BasicShapes/Cube"), FVector(150.f, 0.f, -25.f), FVector(0.8f, 0.5f, 0.05f), FLinearColor(0.3f, 0.2f, 0.15f));  // war table
		SpawnProp(TEXT("/Engine/BasicShapes/Cube"), FVector(150.f, 0.f, -18.f), FVector(0.6f, 0.4f, 0.005f), FLinearColor(0.7f, 0.65f, 0.5f));  // map
		SpawnProp(TEXT("/Engine/BasicShapes/Cube"), FVector(100.f, -80.f, -10.f), FVector(0.05f, 0.3f, 0.6f), FLinearColor(0.25f, 0.15f, 0.1f));  // weapon rack
		SpawnPropLight(FVector(150.f, 0.f, 20.f), FLinearColor(1.0f, 0.7f, 0.3f), 2500.f, 300.f);  // torch
		break;

	case ETartariaSpecialist::Surveyor:
		// Tripod (thin cylinder), lens (small sphere), sample box (cube)
		SpawnProp(TEXT("/Engine/BasicShapes/Cylinder"), FVector(160.f, 0.f, -15.f), FVector(0.03f, 0.03f, 0.5f), FLinearColor(0.4f, 0.35f, 0.3f));  // tripod
		SpawnProp(TEXT("/Engine/BasicShapes/Sphere"), FVector(160.f, 0.f, 35.f), FVector(0.08f, 0.08f, 0.08f), FLinearColor(0.6f, 0.8f, 0.9f));  // lens
		SpawnProp(TEXT("/Engine/BasicShapes/Cube"), FVector(120.f, 70.f, -40.f), FVector(0.25f, 0.2f, 0.15f), FLinearColor(0.35f, 0.25f, 0.15f));  // sample box
		break;

	case ETartariaSpecialist::Governor:
		// Throne (tall cube), banner (thin tall cube), scroll pile (small cubes)
		SpawnProp(TEXT("/Engine/BasicShapes/Cube"), FVector(-80.f, 0.f, -10.f), FVector(0.5f, 0.5f, 0.7f), FLinearColor(0.4f, 0.25f, 0.5f));  // throne
		SpawnProp(TEXT("/Engine/BasicShapes/Cube"), FVector(-80.f, 80.f, 20.f), FVector(0.02f, 0.3f, 0.8f), FLinearColor(0.7f, 0.5f, 0.2f));  // banner
		SpawnProp(TEXT("/Engine/BasicShapes/Cube"), FVector(100.f, -40.f, -35.f), FVector(0.2f, 0.15f, 0.1f), FLinearColor(0.8f, 0.75f, 0.6f));  // scroll pile
		SpawnPropLight(FVector(-80.f, 0.f, 60.f), FLinearColor(0.85f, 0.7f, 0.3f), 3000.f, 350.f);  // royal light
		break;

	default: // Steward
		SpawnProp(TEXT("/Engine/BasicShapes/Cube"), FVector(140.f, 0.f, -30.f), FVector(0.5f, 0.3f, 0.04f), FLinearColor(0.4f, 0.3f, 0.2f));  // counter
		SpawnProp(TEXT("/Engine/BasicShapes/Cube"), FVector(140.f, -15.f, -15.f), FVector(0.12f, 0.08f, 0.2f), FLinearColor(0.45f, 0.35f, 0.2f));  // ledger
		break;
	}
}

FString ATartariaNPC::GetSpecialistString() const
{
	switch (SpecialistType)
	{
	case ETartariaSpecialist::ForgeMaster: return TEXT("forge_master");
	case ETartariaSpecialist::Scribe:      return TEXT("scribe");
	case ETartariaSpecialist::Alchemist:   return TEXT("alchemist");
	case ETartariaSpecialist::WarCaptain:  return TEXT("war_captain");
	case ETartariaSpecialist::Surveyor:    return TEXT("surveyor");
	case ETartariaSpecialist::Governor:    return TEXT("governor");
	default:                               return TEXT("steward");
	}
}

// -------------------------------------------------------
// Action Gesture — triggered by dialogue action dispatch
// -------------------------------------------------------

void ATartariaNPC::PlayActionGesture(const FString& ActionType)
{
	if (bPlayingGesture) return;
	bPlayingGesture = true;
	ActionGestureTimer = 0.f;
	ActionGestureDuration = 2.0f;
	CurrentGestureType = ActionType;
}

void ATartariaNPC::SetMood(int32 NewMood)
{
	MoodLevel = FMath::Clamp(NewMood, -1, 2);

	if (!MoodLight) return;

	switch (MoodLevel)
	{
	case -1:  // Displeased
		MoodLight->SetLightColor(FLinearColor(0.8f, 0.2f, 0.1f));
		MoodLight->SetIntensity(800.f);
		break;
	case 0:   // Neutral
		MoodLight->SetLightColor(FLinearColor(0.7f, 0.7f, 0.5f));
		MoodLight->SetIntensity(500.f);
		break;
	case 1:   // Happy
		MoodLight->SetLightColor(FLinearColor(0.3f, 0.9f, 0.3f));
		MoodLight->SetIntensity(800.f);
		break;
	case 2:   // Busy/Working
		MoodLight->SetLightColor(FLinearColor(0.3f, 0.5f, 0.9f));
		MoodLight->SetIntensity(600.f);
		break;
	}
}

void ATartariaNPC::OnInteract_Implementation(APlayerController* Interactor)
{
	UE_LOG(LogTemp, Log, TEXT("TartariaNPC: Player interacted with %s (%s)"),
		*NPCName, *FactionKey);

	UTartariaSoundManager::PlayNPCGreeting(this);

	// Fire delegates so GameMode can show dialogue widget
	OnDialogueStartedDelegate.Broadcast(NPCName, FactionKey);

	SetMood(2);  // Mark as busy during conversation

	// Send initial greeting dialogue
	SendDialogueRequest(Interactor, TEXT("Greetings."));

	// Fire Blueprint event for custom UI
	OnDialogueStarted(Interactor, TEXT("{}"));
}

FString ATartariaNPC::GetInteractPrompt_Implementation() const
{
	return FString::Printf(TEXT("Speak with %s"), *NPCName);
}

void ATartariaNPC::SendDialogueRequest(APlayerController* Interactor, const FString& PlayerMessage)
{
	UHJGameInstance* GI = UHJGameInstance::Get(this);
	if (!GI || !GI->ApiClient)
	{
		OnDialogueError();
		return;
	}

	FString SpecialistStr = GetSpecialistString();

	// First fetch forge context so NPC can reference recent builds / overnight results
	TWeakObjectPtr<ATartariaNPC> WeakThis(this);
	FString Msg = PlayerMessage;

	FOnApiResponse CtxCB;
	CtxCB.BindLambda([WeakThis, Msg, SpecialistStr](bool bCtxOk, const FString& CtxBody)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, bCtxOk, CtxBody, Msg, SpecialistStr]()
		{
			ATartariaNPC* Self = WeakThis.Get();
			if (!Self) return;

			UHJGameInstance* GI2 = UHJGameInstance::Get(Self);
			if (!GI2 || !GI2->ApiClient)
			{
				Self->OnDialogueError();
				return;
			}

			// Build context string from forge data
			FString ForgeContext;
			if (bCtxOk && !CtxBody.IsEmpty())
			{
				TSharedPtr<FJsonObject> CtxJson;
				TSharedRef<TJsonReader<>> CtxReader = TJsonReaderFactory<>::Create(CtxBody);
				if (FJsonSerializer::Deserialize(CtxReader, CtxJson) && CtxJson.IsValid())
				{
					// Extract recent project names
					const TArray<TSharedPtr<FJsonValue>>* Projects;
					if (CtxJson->TryGetArrayField(TEXT("recent_projects"), Projects) && Projects->Num() > 0)
					{
						TArray<FString> Names;
						for (const TSharedPtr<FJsonValue>& V : *Projects)
						{
							TSharedPtr<FJsonObject> P = V->AsObject();
							if (P.IsValid())
							{
								FString Name;
								if (P->TryGetStringField(TEXT("name"), Name))
									Names.Add(Name);
							}
						}
						if (Names.Num() > 0)
						{
							ForgeContext += TEXT("Recent forge completions: ") + FString::Join(Names, TEXT(", ")) + TEXT(". ");
						}
					}

					// Extract overnight summary
					const TSharedPtr<FJsonObject>* OvernightObj;
					if (CtxJson->TryGetObjectField(TEXT("overnight_summary"), OvernightObj) && OvernightObj->IsValid())
					{
						int32 OvCompleted = 0;
						(*OvernightObj)->TryGetNumberField(TEXT("projects_completed"), OvCompleted);
						if (OvCompleted > 0)
						{
							ForgeContext += FString::Printf(TEXT("The overnight forge completed %d project(s). "), OvCompleted);
						}
					}
				}
			}

			// Build JSON body
			TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject());
			Body->SetStringField(TEXT("npc_name"), Self->NPCName);
			Body->SetStringField(TEXT("faction"), Self->FactionKey);
			Body->SetStringField(TEXT("persona"), Self->PersonaPrompt);
			Body->SetStringField(TEXT("message"), Msg);
			Body->SetStringField(TEXT("specialist_type"), SpecialistStr);
			if (!ForgeContext.IsEmpty())
			{
				Body->SetStringField(TEXT("forge_context"), ForgeContext);
			}

			FString BodyStr;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
			FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

			TWeakObjectPtr<ATartariaNPC> WeakSelf2(Self);

			FOnApiResponse CB;
			CB.BindLambda([WeakSelf2](bool bOk, const FString& RespBody)
			{
				AsyncTask(ENamedThreads::GameThread, [WeakSelf2, bOk, RespBody]()
				{
					ATartariaNPC* S = WeakSelf2.Get();
					if (!S) return;

					if (!bOk)
					{
						S->OnDialogueErroredDelegate.Broadcast(S->NPCName);
						S->OnDialogueError();
						return;
					}

					// Parse response
					TSharedPtr<FJsonObject> Json;
					TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RespBody);
					if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
					{
						FString Response;
						if (Json->TryGetStringField(TEXT("response"), Response))
						{
							TArray<FString> Actions;
							const TArray<TSharedPtr<FJsonValue>>* ActionsArray;
							if (Json->TryGetArrayField(TEXT("available_actions"), ActionsArray))
							{
								for (const TSharedPtr<FJsonValue>& Val : *ActionsArray)
								{
									FString ActionStr;
									if (Val->TryGetString(ActionStr))
										Actions.Add(ActionStr);
								}
							}

							S->OnDialogueReceivedDelegate.Broadcast(
								S->NPCName, Response, Actions);
							S->OnDialogueResponse(Response);

							// Trigger action gesture based on available actions
							if (Actions.Num() > 0)
							{
								S->PlayActionGesture(Actions[0]);
							}
							S->SetMood(1);  // Happy after successful response
						}
						else
						{
							S->OnDialogueErroredDelegate.Broadcast(S->NPCName);
							S->OnDialogueError();
						}
					}
					else
					{
						S->OnDialogueErroredDelegate.Broadcast(S->NPCName);
						S->OnDialogueError();
					}
				});
			});

			GI2->ApiClient->Post(Self->DialogueEndpoint, BodyStr, CB);
		});
	});

	// Fire async fetch for forge context (non-blocking; if it fails, dialogue proceeds without context)
	GI->ApiClient->Get(TEXT("/api/engineering/forge-context"), CtxCB);
}
