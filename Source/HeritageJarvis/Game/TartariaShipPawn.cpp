#include "TartariaShipPawn.h"
#include "TartariaCharacter.h"
#include "Core/HJApiClient.h"
#include "Core/HJGameInstance.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

ATartariaShipPawn::ATartariaShipPawn()
{
    PrimaryActorTick.bCanEverTick = true;

    // ── Root: primary cylinder hull ──────────────────────────────────────
    ShipBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShipBody"));
    SetRootComponent(ShipBody);
    ShipBody->SetCollisionProfileName(TEXT("Pawn"));

    UStaticMesh* CylMesh = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (CylMesh)
        ShipBody->SetStaticMesh(CylMesh);

    // Hull scale: rotate cylinder so its long axis runs along +X (forward)
    // Cylinder in UE is aligned along Z; rotate 90° on Y to reorient it.
    ShipBody->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));  // pitch 90 → nose forward
    ShipBody->SetRelativeScale3D(FVector(3.0f, 1.5f, 1.5f));      // long hull, squat cross-section

    UMaterialInstanceDynamic* HullMat = ShipBody->CreateAndSetMaterialInstanceDynamic(0);
    if (HullMat)
    {
        HullMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.18f, 0.16f, 0.12f)); // dark bronze
    }

    // ── Nose cone ────────────────────────────────────────────────────────
    NoseCone = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NoseCone"));
    NoseCone->SetupAttachment(ShipBody);
    UStaticMesh* ConeMesh = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Engine/BasicShapes/Cone.Cone"));
    if (ConeMesh)
        NoseCone->SetStaticMesh(ConeMesh);
    // Attach at the +X (forward) tip of the hull cylinder.
    // Cylinder centre is at 0 but its half-length in local space is ~50 units before scale.
    // After 3x scale along X and 90° pitch: forward tip is +150 cm in world X.
    // We use local coords relative to ShipBody:
    NoseCone->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));    // Z in local = X in world after rotation
    NoseCone->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
    NoseCone->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.5f));
    NoseCone->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    UMaterialInstanceDynamic* NoseMat = NoseCone->CreateAndSetMaterialInstanceDynamic(0);
    if (NoseMat)
    {
        NoseMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.55f, 0.45f, 0.20f)); // gold trim
    }

    // ── Port wing plate (flat cube to the left) ───────────────────────────
    WingPortPlate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WingPortPlate"));
    WingPortPlate->SetupAttachment(ShipBody);
    UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh)
        WingPortPlate->SetStaticMesh(CubeMesh);
    WingPortPlate->SetRelativeLocation(FVector(-80.0f, 0.0f, 0.0f));  // left of hull
    WingPortPlate->SetRelativeScale3D(FVector(1.8f, 0.08f, 0.5f));    // wide, very flat, half-hull depth
    WingPortPlate->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    UMaterialInstanceDynamic* WingPMat = WingPortPlate->CreateAndSetMaterialInstanceDynamic(0);
    if (WingPMat)
    {
        WingPMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.12f, 0.10f, 0.08f));
    }

    // ── Starboard wing plate ──────────────────────────────────────────────
    WingStarboardPlate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WingStarboardPlate"));
    WingStarboardPlate->SetupAttachment(ShipBody);
    if (CubeMesh)
        WingStarboardPlate->SetStaticMesh(CubeMesh);
    WingStarboardPlate->SetRelativeLocation(FVector(80.0f, 0.0f, 0.0f));  // right of hull
    WingStarboardPlate->SetRelativeScale3D(FVector(1.8f, 0.08f, 0.5f));
    WingStarboardPlate->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    UMaterialInstanceDynamic* WingSMat = WingStarboardPlate->CreateAndSetMaterialInstanceDynamic(0);
    if (WingSMat)
    {
        WingSMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.12f, 0.10f, 0.08f));
    }

    // ── Engine glow light (behind hull) ───────────────────────────────────
    EngineThrustLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("EngineThrustLight"));
    EngineThrustLight->SetupAttachment(ShipBody);
    EngineThrustLight->SetRelativeLocation(FVector(0.0f, 0.0f, -100.0f));  // aft end in local Z
    EngineThrustLight->SetLightColor(FLinearColor(0.4f, 0.6f, 1.0f));       // blue-white
    EngineThrustLight->Intensity = 0.0f;       // starts off; ramped up by Tick when thrusting
    EngineThrustLight->AttenuationRadius = 800.0f;
    EngineThrustLight->SetCastShadows(false);

    // ── Spring arm — third-person camera ────────────────────────────────
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength         = 500.0f;
    CameraBoom->bUsePawnControlRotation = true;
    CameraBoom->bEnableCameraLag        = true;
    CameraBoom->CameraLagSpeed          = 5.0f;
    CameraBoom->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f));  // slight upward offset

    // ── Follow camera ────────────────────────────────────────────────────
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;
    FollowCamera->SetActive(true);

    // ── Cockpit camera (first person) ────────────────────────────────────
    CockpitCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CockpitCamera"));
    CockpitCamera->SetupAttachment(ShipBody);
    CockpitCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));  // inside canopy in local Z
    CockpitCamera->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f)); // face forward (undo hull rotation)
    CockpitCamera->bUsePawnControlRotation = true;
    CockpitCamera->SetActive(false);  // starts hidden; toggled by ToggleCameraView()

    // ── Pawn settings ─────────────────────────────────────────────────────
    AutoPossessPlayer = EAutoReceiveInput::Disabled;  // boarding is explicit
    bUseControllerRotationYaw   = false;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll  = false;
}

// ---------------------------------------------------------------------------
// BeginPlay
// ---------------------------------------------------------------------------

void ATartariaShipPawn::BeginPlay()
{
    Super::BeginPlay();

    // Fetch initial ship stats from the backend so our HUD values are current.
    if (UHJGameInstance* GI = Cast<UHJGameInstance>(GetGameInstance()))
    {
        if (UHJApiClient* Api = GI->GetApiClient())
        {
            Api->Get(TEXT("/api/game/ship/status"),
                FOnApiResponse::CreateUObject(this, &ATartariaShipPawn::SyncFromBackend));
        }
    }
}

// ---------------------------------------------------------------------------
// SetupPlayerInputComponent
// ---------------------------------------------------------------------------

void ATartariaShipPawn::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);

    // Axis bindings — all configured in DefaultInput.ini (no asset required)
    Input->BindAxis("MoveForward",  this, &ATartariaShipPawn::ThrustForward);
    Input->BindAxis("MoveRight",    this, &ATartariaShipPawn::YawInput);
    Input->BindAxis("Turn",         this, &ATartariaShipPawn::YawInput);
    Input->BindAxis("LookUp",       this, &ATartariaShipPawn::PitchInput);
    Input->BindAxis("ShipRoll",     this, &ATartariaShipPawn::RollInput);

    // Action bindings
    Input->BindAction("Interact",      IE_Pressed, this, &ATartariaShipPawn::OnInteractInput);
    Input->BindAction("ToggleCamera",  IE_Pressed, this, &ATartariaShipPawn::ToggleCameraView);
}

// ---------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------

void ATartariaShipPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // ── Velocity from thrust ──────────────────────────────────────────────
    if (FMath::Abs(ThrottleInput) > KINDA_SMALL_NUMBER)
    {
        CurrentSpeed += ThrottleInput * ThrustPower * DeltaTime;
    }
    else
    {
        // Space drag — bleed speed toward zero at 2% per frame
        CurrentSpeed *= FMath::Pow(0.98f, DeltaTime * 60.0f);
    }

    // Clamp to max speed; allow reverse up to 25% of max
    CurrentSpeed = FMath::Clamp(CurrentSpeed, -MaxSpeed * 0.25f, MaxSpeed);

    // Move along forward vector
    if (FMath::Abs(CurrentSpeed) > 1.0f)
    {
        FVector Movement = GetActorForwardVector() * CurrentSpeed * DeltaTime;
        AddActorWorldOffset(Movement, true);
    }

    // ── Rotation ─────────────────────────────────────────────────────────
    if (FMath::Abs(YawRate) > KINDA_SMALL_NUMBER)
    {
        AddActorLocalRotation(FRotator(0.0f, YawRate * RotationSpeed * DeltaTime, 0.0f), true);
    }
    if (FMath::Abs(PitchRate) > KINDA_SMALL_NUMBER)
    {
        AddActorLocalRotation(FRotator(PitchRate * RotationSpeed * 0.6f * DeltaTime, 0.0f, 0.0f), true);
    }
    if (FMath::Abs(RollRate) > KINDA_SMALL_NUMBER)
    {
        AddActorLocalRotation(FRotator(0.0f, 0.0f, RollRate * RotationSpeed * 0.6f * DeltaTime), true);
    }

    // ── Engine glow intensity — proportional to throttle ─────────────────
    float ThrottleAbs = FMath::Abs(ThrottleInput);
    float TargetIntensity = ThrottleAbs * 5000.0f;
    float CurrentIntensity = EngineThrustLight->Intensity;
    EngineThrustLight->Intensity = FMath::FInterpTo(CurrentIntensity, TargetIntensity, DeltaTime, 8.0f);

    // ── Client-side fuel estimate ─────────────────────────────────────────
    if (ThrottleAbs > KINDA_SMALL_NUMBER && CurrentFuel > 0.0f)
    {
        FuelBurnTimer += DeltaTime;
        // Burn 0.1 units of fuel per second of thrusting (rough estimate)
        float ClientBurn = 0.1f * ThrottleAbs * DeltaTime;
        CurrentFuel = FMath::Max(0.0f, CurrentFuel - ClientBurn);
    }

    // ── Periodic backend sync (every FuelSyncInterval seconds) ───────────
    FuelSyncTimer += DeltaTime;
    if (FuelSyncTimer >= FuelSyncInterval)
    {
        FuelSyncTimer = 0.0f;
        if (UHJGameInstance* GI = Cast<UHJGameInstance>(GetGameInstance()))
        {
            if (UHJApiClient* Api = GI->GetApiClient())
            {
                Api->Get(TEXT("/api/game/ship/status"),
                    FOnApiResponse::CreateUObject(this, &ATartariaShipPawn::SyncFromBackend));
            }
        }
    }

    // ── Reset per-frame input accumulators ───────────────────────────────
    ThrottleInput = 0.0f;
    YawRate       = 0.0f;
    PitchRate     = 0.0f;
    RollRate      = 0.0f;

    // ── HUD overlay update ────────────────────────────────────────────────
    UpdateHUD();
}

// ---------------------------------------------------------------------------
// Input handlers
// ---------------------------------------------------------------------------

void ATartariaShipPawn::ThrustForward(float Value)
{
    // W (positive) = forward thrust; S (negative) = brake / reverse
    ThrottleInput = Value;
}

void ATartariaShipPawn::YawInput(float Value)
{
    // A/D keys and mouse X provide yaw
    YawRate = Value;
}

void ATartariaShipPawn::PitchInput(float Value)
{
    // Mouse Y provides pitch
    PitchRate = Value;
}

void ATartariaShipPawn::RollInput(float Value)
{
    // Q/E provide roll — mapped to "ShipRoll" axis in DefaultInput.ini
    RollRate = Value;
}

void ATartariaShipPawn::OnInteractInput()
{
    // When near a station and pressing E, disembark the ship.
    // The actual docking call to the Python backend is issued by the
    // TartariaWorldSubsystem or the station actor; here we just unpossess.
    Disembark();
}

// ---------------------------------------------------------------------------
// Camera toggle
// ---------------------------------------------------------------------------

void ATartariaShipPawn::ToggleCameraView()
{
    bCockpitView = !bCockpitView;

    FollowCamera->SetActive(!bCockpitView);
    CockpitCamera->SetActive(bCockpitView);

    // In cockpit mode the boom should not control rotation; mouse directly
    // controls the cockpit camera via bUsePawnControlRotation = true on that camera.
    CameraBoom->bUsePawnControlRotation = !bCockpitView;
}

// ---------------------------------------------------------------------------
// Boarding / Disembarking
// ---------------------------------------------------------------------------

void ATartariaShipPawn::Board(ATartariaCharacter* Pilot)
{
    if (!Pilot) return;
    PilotCharacter = Pilot;

    // Hide the character mesh so it doesn't clip through the cockpit
    Pilot->SetActorHiddenInGame(true);
    Pilot->SetActorEnableCollision(false);

    // Transfer control: possess this pawn
    APlayerController* PC = Cast<APlayerController>(Pilot->GetController());
    if (PC)
    {
        PC->Possess(this);
    }

    // Post launch intent to backend
    if (UHJGameInstance* GI = Cast<UHJGameInstance>(GetGameInstance()))
    {
        if (UHJApiClient* Api = GI->GetApiClient())
        {
            FString Body = TEXT("{\"body_id\":\"earth\"}");
            Api->Post(TEXT("/api/game/ship/launch"), Body,
                FOnApiResponse::CreateLambda([this](bool bOk, const FString& Resp)
                {
                    if (bOk) SyncFromBackend(Resp);
                }));
        }
    }
}

void ATartariaShipPawn::Disembark()
{
    if (!PilotCharacter) return;

    // Restore the character near the ship
    PilotCharacter->SetActorLocation(GetActorLocation() + FVector(0.0f, 200.0f, 0.0f));
    PilotCharacter->SetActorHiddenInGame(false);
    PilotCharacter->SetActorEnableCollision(true);

    // Return control to the character
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (PC)
    {
        PC->Possess(PilotCharacter);
    }

    PilotCharacter = nullptr;

    // Notify the backend to record the docked state
    if (UHJGameInstance* GI = Cast<UHJGameInstance>(GetGameInstance()))
    {
        if (UHJApiClient* Api = GI->GetApiClient())
        {
            FString Body = TEXT("{\"body_id\":\"earth\",\"zone_id\":\"main_port\"}");
            Api->Post(TEXT("/api/game/ship/dock"), Body,
                FOnApiResponse::CreateLambda([this](bool bOk, const FString& Resp)
                {
                    if (bOk)
                    {
                        SyncFromBackend(Resp);
                        OnShipDocked.Broadcast(TEXT("earth"));
                    }
                }));
        }
    }
}

// ---------------------------------------------------------------------------
// HUD update
// ---------------------------------------------------------------------------

void ATartariaShipPawn::UpdateHUD()
{
    // Heading: yaw in 0-359 range
    float Heading = FMath::Fmod(GetActorRotation().Yaw + 360.0f, 360.0f);

    // Speed in km/s for display (CurrentSpeed is cm/s in UE units)
    float SpeedKms = CurrentSpeed * 0.001f;

    // Fuel percent
    float FuelPct = MaxFuel > 0.0f ? (CurrentFuel / MaxFuel * 100.0f) : 0.0f;

    // These values are exposed via BlueprintReadOnly properties so Blueprint
    // HUD widgets can read them directly.  No explicit widget call needed here
    // unless a C++ HUD class is wired up.
    (void)Heading;
    (void)SpeedKms;
    (void)FuelPct;
}

// ---------------------------------------------------------------------------
// Backend sync
// ---------------------------------------------------------------------------

void ATartariaShipPawn::SyncFromBackend(const FString& JsonPayload)
{
    TSharedPtr<FJsonObject> Json;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonPayload);
    if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
        return;

    // Pull scalars from the JSON; use current values as fallbacks
    HullHP     = (float)Json->GetNumberField(TEXT("hull_hp"));
    MaxHullHP  = (float)Json->GetNumberField(TEXT("max_hull_hp"));
    ShieldHP   = (float)Json->GetNumberField(TEXT("shield_hp"));
    MaxShieldHP= (float)Json->GetNumberField(TEXT("max_shield_hp"));
    CurrentFuel= (float)Json->GetNumberField(TEXT("fuel"));
    MaxFuel    = (float)Json->GetNumberField(TEXT("max_fuel"));
    FlightState= Json->GetStringField(TEXT("flight_state"));

    // Broadcast damage notification if HP has dropped significantly
    if (HullHP < MaxHullHP * 0.5f || ShieldHP < MaxShieldHP * 0.5f)
    {
        OnShipDamaged.Broadcast(HullHP, ShieldHP);
    }
}
