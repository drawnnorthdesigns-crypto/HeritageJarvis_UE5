#include "TartariaNPC.h"
#include "Core/HJGameInstance.h"
#include "Core/HJApiClient.h"
#include "Core/TartariaSoundManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/WidgetComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Async/Async.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/Character.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "TimerManager.h"

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

	// ── Reputation Aura Light ─────────────────────────────────
	ReputationAura = CreateDefaultSubobject<UPointLightComponent>(TEXT("ReputationAura"));
	ReputationAura->SetupAttachment(RootComponent);
	ReputationAura->SetRelativeLocation(FVector(0.f, 0.f, 80.f));  // Slightly above NPC center
	ReputationAura->SetIntensity(200.f);
	ReputationAura->SetAttenuationRadius(150.f);
	ReputationAura->SetLightColor(FLinearColor(0.5f, 0.5f, 0.5f));  // Default grey (STRANGER)
	ReputationAura->SetCastShadows(false);

	// ── Speech Bubble (UWidgetComponent — Screen space, faces camera) ──
	SpeechBubble = CreateDefaultSubobject<UWidgetComponent>(TEXT("SpeechBubble"));
	SpeechBubble->SetupAttachment(RootComponent);
	SpeechBubble->SetRelativeLocation(FVector(0.f, 0.f, 140.f));  // Above name tag
	SpeechBubble->SetDrawSize(FVector2D(300.f, 100.f));
	SpeechBubble->SetWidgetSpace(EWidgetSpace::Screen);
	SpeechBubble->SetVisibility(false);  // Hidden until ShowSpeechBubble()
	SpeechBubble->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ATartariaNPC::BeginPlay()
{
	Super::BeginPlay();

	// Apply specialist-specific colors and spawn accessory now that
	// SpecialistType has been set by the editor or WorldPopulator.
	ApplySpecialistAppearance();

	// Initialize emissive dynamic materials on all body meshes
	InitBodyEmissiveMaterials();

	// ── Programmatic Speech Bubble Widget ─────────────────────
	// Create a UUserWidget at runtime with a styled text block
	if (SpeechBubble)
	{
		UWorld* World = GetWorld();
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		if (PC)
		{
			UUserWidget* BubbleWidget = CreateWidget<UUserWidget>(PC);
			if (BubbleWidget)
			{
				// Create a border for background styling
				UBorder* BubbleBorder = NewObject<UBorder>(BubbleWidget);
				BubbleBorder->SetBrushColor(FLinearColor(0.05f, 0.04f, 0.03f, 0.85f));  // Dark parchment
				BubbleBorder->SetPadding(FMargin(12.f, 8.f, 12.f, 8.f));

				// Create the text block inside the border
				SpeechTextBlock = NewObject<UTextBlock>(BubbleWidget);
				SpeechTextBlock->SetText(FText::GetEmpty());
				SpeechTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.85f, 0.7f)));  // Warm gold text

				FSlateFontInfo FontInfo = SpeechTextBlock->GetFont();
				FontInfo.Size = 14;
				SpeechTextBlock->SetFont(FontInfo);

				SpeechTextBlock->SetAutoWrapText(true);

				// Assemble: border contains text block
				BubbleBorder->AddChild(SpeechTextBlock);

				// Set the border as the root widget of the UUserWidget
				BubbleWidget->SetRootWidget(BubbleBorder);

				SpeechBubble->SetWidget(BubbleWidget);
			}
		}
	}

	// Spawn environmental workshop props around the NPC
	SpawnWorkshopProps();

	// Fetch initial reputation tier from backend (non-blocking)
	FetchReputationTier();

	// Fetch initial activity state from backend (non-blocking)
	FetchCurrentActivity();
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

		// ── Dialogue Eye Contact (enhanced precision head tracking) ──
		if (bInDialogue)
		{
			bPlayerNearby = true;

			// Turn body toward player
			FVector ToPlayer2D = (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			float BodyYaw = FMath::Atan2(ToPlayer2D.Y, ToPlayer2D.X) * (180.f / PI);
			FRotator BodyTargetRot = FRotator(0.f, BodyYaw, 0.f);
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), BodyTargetRot, DeltaTime, 3.f));

			// Calculate head rotation toward player camera location
			APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(this, 0);
			FVector LookTarget = CamMgr ? CamMgr->GetCameraLocation() : Player->GetActorLocation();
			FVector HeadWorldLoc = BodyHead->GetComponentLocation();
			FVector ToCamera = (LookTarget - HeadWorldLoc).GetSafeNormal();

			// Convert to local-space rotator relative to actor forward
			FRotator WorldLookRot = ToCamera.Rotation();
			FRotator ActorRot = GetActorRotation();
			FRotator LocalLookRot = WorldLookRot - ActorRot;
			LocalLookRot.Normalize();

			// Clamp to max 30 degrees yaw, 15 degrees pitch
			DialogueTargetHeadRotation.Yaw = FMath::Clamp(LocalLookRot.Yaw, -30.f, 30.f);
			DialogueTargetHeadRotation.Pitch = FMath::Clamp(LocalLookRot.Pitch, -15.f, 15.f);
			DialogueTargetHeadRotation.Roll = 0.f;

			// Smooth lerp at ~3 degrees/sec equivalent (InterpSpeed=3.0)
			DialogueCurrentHeadRotation = FMath::RInterpTo(
				DialogueCurrentHeadRotation, DialogueTargetHeadRotation, DeltaTime, 3.0f);

			BodyHead->SetRelativeRotation(DialogueCurrentHeadRotation);

			// Greeting gesture: raise right arm when player approaches
			if (DistToPlayer < 300.f && BodyArmR)
			{
				float GestureAngle = FMath::FInterpTo(
					BodyArmR->GetRelativeRotation().Pitch, -30.f, DeltaTime, 4.f);
				BodyArmR->SetRelativeRotation(FRotator(GestureAngle, 0.f, 15.f));
			}
		}
		else if (DistToPlayer < 500.f)  // Within interaction range (non-dialogue)
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

			// When dialogue just ended, smoothly lerp head back from dialogue rotation
			DialogueCurrentHeadRotation = FMath::RInterpTo(
				DialogueCurrentHeadRotation, FRotator(HeadPitch, 0.f, 0.f), DeltaTime, 2.f);
			BodyHead->SetRelativeRotation(DialogueCurrentHeadRotation);

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
			// Reset head when player is far — lerp back to forward-facing
			DialogueCurrentHeadRotation = FMath::RInterpTo(
				DialogueCurrentHeadRotation, FRotator::ZeroRotator, DeltaTime, 2.f);
			BodyHead->SetRelativeRotation(DialogueCurrentHeadRotation);
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

	// ── Activity State Visuals (overrides idle variation when not in dialogue/gesture) ──
	if (!bPlayingGesture && !bPlayerNearby && !bInDialogue)
	{
		UpdateActivityVisuals(DeltaTime);
	}
	else if (!bPlayingGesture && !bPlayerNearby)
	{
		// ── Idle Variation System (fallback when in dialogue but player walked away) ──
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

	// ── Periodic Activity Re-fetch ────────────────────────────
	{
		UWorld* W = GetWorld();
		if (W)
		{
			float WorldTime = W->GetTimeSeconds();
			// Re-fetch activity every 15 seconds (lightweight HTTP call)
			if (WorldTime - LastActivityFetchTime >= 15.f)
			{
				FetchCurrentActivity();
			}
		}
	}

	// ── Dialogue Request Timeout Guard ──────────────────────
	if (bDialogueRequestPending)
	{
		DialogueRequestTimer += DeltaTime;
		if (DialogueRequestTimer >= DialogueRequestTimeout)
		{
			OnDialogueTimeout();
		}
	}

	// ── Reputation Aura Pulse ────────────────────────────────
	if (ReputationAura)
	{
		AuraPulsePhase += DeltaTime * 0.5f * 2.f * PI;  // 0.5 Hz
		if (AuraPulsePhase > 2.f * PI)
			AuraPulsePhase -= 2.f * PI;

		// +/- 20% intensity oscillation
		float PulseFactor = 1.0f + FMath::Sin(AuraPulsePhase) * 0.2f;
		ReputationAura->SetIntensity(ReputationBaseIntensity * PulseFactor);
	}

	// ── Billboard: NameTag faces player camera ──
	if (NameTag)
	{
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (PC)
		{
			FVector CamLoc;
			FRotator CamRot;
			PC->GetPlayerViewPoint(CamLoc, CamRot);
			FRotator LookAt = UKismetMathLibrary::FindLookAtRotation(
				NameTag->GetComponentLocation(), CamLoc);
			NameTag->SetWorldRotation(LookAt);
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

// -------------------------------------------------------
// Dialogue Eye Contact
// -------------------------------------------------------

void ATartariaNPC::StartDialogue()
{
	bInDialogue = true;
	// Fetch reputation tier when dialogue begins
	FetchReputationTier();
}

void ATartariaNPC::EndDialogue()
{
	bInDialogue = false;
	// DialogueCurrentHeadRotation will lerp back to forward in Tick()
}

// -------------------------------------------------------
// Reputation Aura
// -------------------------------------------------------

FLinearColor ATartariaNPC::GetReputationTierColor(int32 Tier)
{
	switch (Tier)
	{
	case 0:  return FLinearColor(0.5f, 0.5f, 0.5f);   // STRANGER — grey
	case 1:  return FLinearColor(0.3f, 0.5f, 0.8f);   // ACQUAINTANCE — blue
	case 2:  return FLinearColor(0.2f, 0.8f, 0.3f);   // ALLY — green
	case 3:  return FLinearColor(0.9f, 0.75f, 0.2f);  // CONFIDANT — gold
	case 4:  return FLinearColor(0.6f, 0.2f, 0.9f);   // TRUSTED — purple
	default: return FLinearColor(0.5f, 0.5f, 0.5f);
	}
}

float ATartariaNPC::GetReputationTierIntensity(int32 Tier)
{
	switch (Tier)
	{
	case 0:  return 200.f;   // STRANGER
	case 1:  return 300.f;   // ACQUAINTANCE
	case 2:  return 500.f;   // ALLY
	case 3:  return 800.f;   // CONFIDANT
	case 4:  return 1200.f;  // TRUSTED
	default: return 200.f;
	}
}

void ATartariaNPC::SetReputationFromScore(int32 Score)
{
	Score = FMath::Clamp(Score, 0, 100);
	if (Score >= 80)
		ReputationTier = 4;  // TRUSTED
	else if (Score >= 60)
		ReputationTier = 3;  // CONFIDANT
	else if (Score >= 40)
		ReputationTier = 2;  // ALLY
	else if (Score >= 20)
		ReputationTier = 1;  // ACQUAINTANCE
	else
		ReputationTier = 0;  // STRANGER

	ApplyReputationAuraVisuals();

	// Update emissive glow on body meshes to match reputation
	UpdateReputationGlow(Score);
}

void ATartariaNPC::ApplyReputationAuraVisuals()
{
	if (!ReputationAura) return;

	FLinearColor AuraColor = GetReputationTierColor(ReputationTier);
	ReputationBaseIntensity = GetReputationTierIntensity(ReputationTier);

	ReputationAura->SetLightColor(AuraColor);
	ReputationAura->SetIntensity(ReputationBaseIntensity);

	// Scale attenuation radius with tier for more visible auras at higher tiers
	float Radius = 100.f + ReputationTier * 30.f;
	ReputationAura->SetAttenuationRadius(Radius);
}

// -------------------------------------------------------
// Emissive Reputation Glow
// -------------------------------------------------------

void ATartariaNPC::InitBodyEmissiveMaterials()
{
	// Collect all body mesh components that should receive the emissive glow
	TArray<UStaticMeshComponent*> BodyMeshes = {
		BodyTorso, BodyHead, BodyArmL, BodyArmR, BodyLegL, BodyLegR
	};

	BodyDynamicMaterials.Empty();

	for (UStaticMeshComponent* Mesh : BodyMeshes)
	{
		if (!Mesh) continue;

		// Get the existing dynamic material (created by ApplyColorToMesh in ApplySpecialistAppearance)
		// or create a new one if it does not exist yet
		UMaterialInstanceDynamic* DynMat = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
		if (!DynMat)
		{
			DynMat = Mesh->CreateAndSetMaterialInstanceDynamic(0);
		}

		if (DynMat)
		{
			// Initialize with dim grey emissive (STRANGER tier)
			DynMat->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(0.1f, 0.1f, 0.1f) * EmissiveStrength);
			BodyDynamicMaterials.Add(DynMat);
		}
	}
}

FLinearColor ATartariaNPC::GetEmissiveTierColor(int32 Score)
{
	Score = FMath::Clamp(Score, 0, 100);

	if (Score >= 80)
		return FLinearColor(1.0f, 0.8f, 0.2f);    // TRUSTED — bright gold
	else if (Score >= 60)
		return FLinearColor(0.6f, 0.45f, 0.1f);    // FRIENDLY — warm gold
	else if (Score >= 40)
		return FLinearColor(0.3f, 0.3f, 0.3f);     // NEUTRAL — soft white
	else if (Score >= 20)
		return FLinearColor(0.1f, 0.1f, 0.3f);     // CAUTIOUS — faint blue
	else
		return FLinearColor(0.1f, 0.1f, 0.1f);     // STRANGER — dim grey
}

void ATartariaNPC::UpdateReputationGlow(int32 ReputationScore)
{
	FLinearColor TierColor = GetEmissiveTierColor(ReputationScore);
	FLinearColor EmissiveValue = TierColor * EmissiveStrength;

	for (UMaterialInstanceDynamic* DynMat : BodyDynamicMaterials)
	{
		if (DynMat)
		{
			DynMat->SetVectorParameterValue(TEXT("EmissiveColor"), EmissiveValue);
		}
	}
}

// -------------------------------------------------------
// Speech Bubble
// -------------------------------------------------------

void ATartariaNPC::ShowSpeechBubble(const FString& Text, float Duration)
{
	CurrentSpeechText = Text;

	// Update the text block content
	if (SpeechTextBlock)
	{
		// Truncate to 80 characters for readability
		FString DisplayText = Text.Len() > 80 ? Text.Left(77) + TEXT("...") : Text;
		SpeechTextBlock->SetText(FText::FromString(DisplayText));
	}

	// Show the widget
	if (SpeechBubble)
	{
		SpeechBubble->SetVisibility(true);
	}

	// Clear any existing hide timer and set a new one
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(SpeechTimerHandle);
		World->GetTimerManager().SetTimer(
			SpeechTimerHandle,
			this,
			&ATartariaNPC::HideSpeechBubble,
			Duration,
			false  // non-looping
		);
	}
}

void ATartariaNPC::HideSpeechBubble()
{
	CurrentSpeechText.Empty();

	if (SpeechBubble)
	{
		SpeechBubble->SetVisibility(false);
	}

	// Clear the timer in case this was called manually
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(SpeechTimerHandle);
	}
}

void ATartariaNPC::FetchReputationTier()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Cache: re-fetch only every 30 seconds
	float CurrentTime = World->GetTimeSeconds();
	if (CurrentTime - LastReputationFetchTime < 30.f)
		return;
	LastReputationFetchTime = CurrentTime;

	UHJGameInstance* GI = UHJGameInstance::Get(this);
	if (!GI || !GI->ApiClient)
		return;

	FString SpecialistStr = GetSpecialistString();
	FString Endpoint = FString::Printf(TEXT("/api/game/npc/profile/%s"), *SpecialistStr);

	TWeakObjectPtr<ATartariaNPC> WeakThis(this);

	FOnApiResponse CB;
	CB.BindLambda([WeakThis](bool bOk, const FString& Body)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, bOk, Body]()
		{
			ATartariaNPC* Self = WeakThis.Get();
			if (!Self || !bOk) return;

			TSharedPtr<FJsonObject> Json;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
			if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
				return;

			// Navigate to profile.relationship_score
			const TSharedPtr<FJsonObject>* ProfileObj;
			if (Json->TryGetObjectField(TEXT("profile"), ProfileObj) && ProfileObj->IsValid())
			{
				int32 Score = 0;
				if ((*ProfileObj)->TryGetNumberField(TEXT("relationship_score"), Score))
				{
					Self->SetReputationFromScore(Score);
				}
			}
		});
	});

	GI->ApiClient->Get(Endpoint, CB);
}

// -------------------------------------------------------
// Activity State — visual reflection of NPC behavior
// -------------------------------------------------------

ENPCActivityState ATartariaNPC::MapActivityStringToState(const FString& Activity)
{
	// Thinking activities: researching, reading, contemplating, analyzing
	if (Activity == TEXT("researching") || Activity == TEXT("reading")
		|| Activity == TEXT("contemplating") || Activity == TEXT("analyzing")
		|| Activity == TEXT("cataloging"))
	{
		return ENPCActivityState::Thinking;
	}

	// Working activities: constructing, repairing, brewing, distilling
	if (Activity == TEXT("constructing") || Activity == TEXT("repairing")
		|| Activity == TEXT("brewing") || Activity == TEXT("distilling"))
	{
		return ENPCActivityState::Working;
	}

	// Social activities: negotiating, appraising, briefing
	// We map these to Thinking as well (head nods handled as special case)
	if (Activity == TEXT("negotiating") || Activity == TEXT("appraising")
		|| Activity == TEXT("briefing"))
	{
		return ENPCActivityState::Thinking;
	}

	// Moving activities: traveling, charting, surveying, observing, calculating
	if (Activity == TEXT("traveling") || Activity == TEXT("charting")
		|| Activity == TEXT("surveying") || Activity == TEXT("observing")
		|| Activity == TEXT("calculating"))
	{
		return ENPCActivityState::Moving;
	}

	return ENPCActivityState::Idle;
}

void ATartariaNPC::UpdateActivityVisuals(float DeltaTime)
{
	// Advance the activity animation timer
	ActivityAnimTimer += DeltaTime;
	if (ActivityAnimTimer > 2.f * PI * 100.f)
	{
		// Prevent float overflow after long sessions
		ActivityAnimTimer -= 2.f * PI * 100.f;
	}

	const FString& Act = CurrentActivity;

	// ── Thinking Activities ─────────────────────────────────
	// researching / reading / contemplating / analyzing / cataloging
	// Head tilts down 10 degrees, subtle side-to-side sway (+/-3 deg, 0.3Hz)
	if (Act == TEXT("researching") || Act == TEXT("reading")
		|| Act == TEXT("contemplating") || Act == TEXT("analyzing")
		|| Act == TEXT("cataloging"))
	{
		if (BodyHead)
		{
			float SwayYaw = FMath::Sin(ActivityAnimTimer * 0.3f * 2.f * PI) * 3.f;
			BodyHead->SetRelativeRotation(FRotator(-10.f, SwayYaw, 0.f));
		}
		// Arms at rest — maybe holding a book
		if (BodyArmL)
		{
			BodyArmL->SetRelativeRotation(FRotator(-20.f, 0.f, -10.f));
		}
		if (BodyArmR)
		{
			BodyArmR->SetRelativeRotation(FRotator(-20.f, 0.f, 10.f));
		}
		return;
	}

	// ── Working Activities ─────────────────────────────────
	// constructing / repairing / brewing / distilling
	// Arm-level mesh oscillation (+/-5 units vertical, 1Hz) — "working" motion
	if (Act == TEXT("constructing") || Act == TEXT("repairing")
		|| Act == TEXT("brewing") || Act == TEXT("distilling"))
	{
		if (BodyArmR)
		{
			float ArmPump = FMath::Sin(ActivityAnimTimer * 1.0f * 2.f * PI) * 25.f;
			BodyArmR->SetRelativeRotation(FRotator(-30.f + ArmPump, 0.f, 10.f));
		}
		if (BodyArmL)
		{
			// Left arm steady, bracing
			BodyArmL->SetRelativeRotation(FRotator(-15.f, 0.f, -5.f));
		}
		// Slight forward lean for working posture
		if (BodyTorso)
		{
			BodyTorso->SetRelativeRotation(FRotator(5.f, 0.f, 0.f));
		}
		// Head looks down at work
		if (BodyHead)
		{
			BodyHead->SetRelativeRotation(FRotator(-8.f, 0.f, 0.f));
		}
		return;
	}

	// ── Social Activities ──────────────────────────────────
	// negotiating / appraising / briefing
	// Slight forward lean (5 degrees), periodic head nods (every 3 seconds)
	if (Act == TEXT("negotiating") || Act == TEXT("appraising")
		|| Act == TEXT("briefing"))
	{
		if (BodyTorso)
		{
			BodyTorso->SetRelativeRotation(FRotator(5.f, 0.f, 0.f));
		}
		if (BodyHead)
		{
			// Periodic nod: every 3 seconds, dip 8 degrees over ~0.5s then back up
			float NodCycle = FMath::Fmod(ActivityAnimTimer, 3.0f);
			float NodPitch = 0.f;
			if (NodCycle < 0.25f)
			{
				// Dipping down
				NodPitch = -8.f * (NodCycle / 0.25f);
			}
			else if (NodCycle < 0.5f)
			{
				// Coming back up
				NodPitch = -8.f * (1.f - (NodCycle - 0.25f) / 0.25f);
			}
			BodyHead->SetRelativeRotation(FRotator(NodPitch, 0.f, 0.f));
		}
		// Hands clasped or gesturing
		if (BodyArmL)
		{
			BodyArmL->SetRelativeRotation(FRotator(-10.f, 5.f, -10.f));
		}
		if (BodyArmR)
		{
			float GestureAngle = FMath::Sin(ActivityAnimTimer * 0.5f) * 8.f;
			BodyArmR->SetRelativeRotation(FRotator(-10.f + GestureAngle, -5.f, 10.f));
		}
		return;
	}

	// ── Moving / Scanning Activities ──────────────────────
	// traveling / charting / surveying / observing / calculating
	// Slow rotation (10 degrees/sec yaw scan) — "looking around"
	if (Act == TEXT("traveling") || Act == TEXT("charting")
		|| Act == TEXT("surveying") || Act == TEXT("observing")
		|| Act == TEXT("calculating"))
	{
		if (BodyHead)
		{
			// Sweep yaw: +/-30 degrees at ~10 deg/s (period ~6 seconds)
			float ScanYaw = FMath::Sin(ActivityAnimTimer * (10.f / 30.f)) * 30.f;
			float ScanPitch = FMath::Sin(ActivityAnimTimer * 0.15f) * 5.f;
			BodyHead->SetRelativeRotation(FRotator(ScanPitch, ScanYaw, 0.f));
		}
		// One arm shading eyes or holding instrument
		if (BodyArmR)
		{
			BodyArmR->SetRelativeRotation(FRotator(-40.f, 10.f, 15.f));
		}
		return;
	}

	// ── Idle (default) ──────────────────────────────────────
	// Gentle breathing sway (+/-1 degree, 0.25Hz)
	{
		if (BodyHead)
		{
			float IdleSway = FMath::Sin(ActivityAnimTimer * 0.25f * 2.f * PI) * 1.f;
			BodyHead->SetRelativeRotation(FRotator(IdleSway, 0.f, 0.f));
		}
		// Reset torso to neutral
		if (BodyTorso)
		{
			BodyTorso->SetRelativeRotation(FRotator::ZeroRotator);
		}
		// Use idle variation system when actually idle
		IdleVariationTimer += DeltaTime;
		if (IdleVariationTimer >= NextIdleSwitchTime)
		{
			IdleVariationTimer = 0.f;
			CurrentIdleVariant = FMath::RandRange(0, 3);
			NextIdleSwitchTime = FMath::RandRange(12.f, 30.f);
		}

		switch (CurrentIdleVariant)
		{
		case 1:  // Look around
			if (BodyHead)
			{
				float LookYaw = FMath::Sin(BreathPhase * 0.3f) * 25.f;
				BodyHead->SetRelativeRotation(FRotator(0.f, LookYaw, 0.f));
			}
			break;
		case 2:  // Inspect tool
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
		case 3:  // Shift weight
			if (BodyTorso)
			{
				float SwayRoll = FMath::Sin(BreathPhase * 0.4f) * 3.f;
				BodyTorso->SetRelativeRotation(FRotator(0.f, 0.f, SwayRoll));
			}
			break;
		default:
			break;
		}
	}
}

void ATartariaNPC::FetchCurrentActivity()
{
	UWorld* World = GetWorld();
	if (!World) return;

	float CurrentTime = World->GetTimeSeconds();
	LastActivityFetchTime = CurrentTime;

	UHJGameInstance* GI = UHJGameInstance::Get(this);
	if (!GI || !GI->ApiClient)
		return;

	FString SpecialistStr = GetSpecialistString();
	FString Endpoint = FString::Printf(TEXT("/api/game/npc/profile/%s"), *SpecialistStr);

	TWeakObjectPtr<ATartariaNPC> WeakThis(this);

	FOnApiResponse CB;
	CB.BindLambda([WeakThis](bool bOk, const FString& Body)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, bOk, Body]()
		{
			ATartariaNPC* Self = WeakThis.Get();
			if (!Self || !bOk) return;

			TSharedPtr<FJsonObject> Json;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
			if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
				return;

			// Navigate to profile.current_activity
			const TSharedPtr<FJsonObject>* ProfileObj;
			if (Json->TryGetObjectField(TEXT("profile"), ProfileObj) && ProfileObj->IsValid())
			{
				FString Activity;
				if ((*ProfileObj)->TryGetStringField(TEXT("current_activity"), Activity))
				{
					Self->CurrentActivity = Activity;
					Self->ActivityState = MapActivityStringToState(Activity);
				}

				// Also update reputation while we have the profile data
				int32 Score = 0;
				if ((*ProfileObj)->TryGetNumberField(TEXT("relationship_score"), Score))
				{
					Self->SetReputationFromScore(Score);
				}
			}
		});
	});

	GI->ApiClient->Get(Endpoint, CB);
}

void ATartariaNPC::OnInteract_Implementation(APlayerController* Interactor)
{
	UE_LOG(LogTemp, Log, TEXT("TartariaNPC: Player interacted with %s (%s)"),
		*NPCName, *FactionKey);

	UTartariaSoundManager::PlayNPCGreeting(this);

	// Fire delegates so GameMode can show dialogue widget
	OnDialogueStartedDelegate.Broadcast(NPCName, FactionKey);

	SetMood(2);  // Mark as busy during conversation

	// Enter dialogue mode — enables eye contact tracking + fetches reputation
	StartDialogue();

	// Send initial greeting dialogue — or queue it if a request is already in-flight
	const FString GreetingMsg = TEXT("Greetings.");
	if (bDialogueRequestPending)
	{
		// Cap queue at 3 entries; discard oldest if full
		if (PendingDialoguePrompts.Num() >= 3)
		{
			UE_LOG(LogTemp, Warning, TEXT("TartariaNPC [%s]: Dialogue queue full — discarding oldest prompt."), *NPCName);
			PendingDialoguePrompts.RemoveAt(0);
		}
		PendingDialoguePrompts.Add(GreetingMsg);
		UE_LOG(LogTemp, Log, TEXT("TartariaNPC [%s]: Request pending — queued prompt '%s' (queue size: %d)."),
			*NPCName, *GreetingMsg, PendingDialoguePrompts.Num());
	}
	else
	{
		SendDialogueRequest(Interactor, GreetingMsg);
	}

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
						S->bDialogueRequestPending = false;
						S->DialogueRequestTimer = 0.f;
						S->OnDialogueErroredDelegate.Broadcast(S->NPCName);
						S->OnDialogueError();
						S->ProcessNextDialoguePrompt();
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

							// Show speech bubble with first sentence (or truncated)
							{
								FString BubbleText = Response;
								// Extract first sentence (up to first period, exclamation, or question mark)
								int32 SentenceEnd = -1;
								for (int32 i = 0; i < BubbleText.Len(); ++i)
								{
									TCHAR Ch = BubbleText[i];
									if (Ch == TEXT('.') || Ch == TEXT('!') || Ch == TEXT('?'))
									{
										SentenceEnd = i + 1;
										break;
									}
								}
								if (SentenceEnd > 0 && SentenceEnd < BubbleText.Len())
								{
									BubbleText = BubbleText.Left(SentenceEnd);
								}
								// ShowSpeechBubble handles the 80-char truncation internally
								S->ShowSpeechBubble(BubbleText, 5.f);
							}

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

					// Release the pending guard and drain the queue
					S->bDialogueRequestPending = false;
					S->DialogueRequestTimer = 0.f;
					S->ProcessNextDialoguePrompt();
				});
			});

			Self->bDialogueRequestPending = true;
			Self->DialogueRequestTimer = 0.f;
			GI2->ApiClient->Post(Self->DialogueEndpoint, BodyStr, CB);
		});
	});

	// Fire async fetch for forge context (non-blocking; if it fails, dialogue proceeds without context)
	GI->ApiClient->Get(TEXT("/api/engineering/forge-context"), CtxCB);
}

// -------------------------------------------------------
// Dialogue Priority Queue
// -------------------------------------------------------

void ATartariaNPC::ProcessNextDialoguePrompt()
{
	if (PendingDialoguePrompts.Num() == 0)
		return;

	// Pop the oldest queued prompt
	FString NextPrompt = PendingDialoguePrompts[0];
	PendingDialoguePrompts.RemoveAt(0);

	UE_LOG(LogTemp, Log, TEXT("TartariaNPC [%s]: Processing queued prompt '%s' (%d remaining)."),
		*NPCName, *NextPrompt, PendingDialoguePrompts.Num());

	// Interactor is not available here; pass nullptr — SendDialogueRequest
	// only uses it to propagate to Blueprint events which accept nullptr safely.
	SendDialogueRequest(nullptr, NextPrompt);
}

void ATartariaNPC::OnDialogueTimeout()
{
	UE_LOG(LogTemp, Warning,
		TEXT("TartariaNPC [%s]: Dialogue request timed out after %.1fs — clearing pending state."),
		*NPCName, DialogueRequestTimeout);

	bDialogueRequestPending = false;
	DialogueRequestTimer = 0.f;

	// Attempt to drain the queue so interaction is not permanently blocked
	ProcessNextDialoguePrompt();
}
