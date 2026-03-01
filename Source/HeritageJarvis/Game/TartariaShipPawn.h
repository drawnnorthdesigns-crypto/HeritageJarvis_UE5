#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "TartariaTypes.h"
#include "TartariaShipPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class ATartariaCharacter;

// ---------------------------------------------------------------------------
// Delegates
// ---------------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnShipDamaged, float, HullHP, float, ShieldHP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShipDocked, FString, BodyId);

/**
 * ATartariaShipPawn — Flyable corvette vessel for Tartaria space transit.
 *
 * Compound primitive mesh (cylinder hull + cone nose + flat wing plates).
 * Supports third-person (spring arm) and first-person (cockpit) cameras.
 * Uses legacy input system — bindings defined in DefaultInput.ini.
 *
 * Boarding flow:
 *   ATartariaCharacter calls Board(this) → player possesses pawn, character hidden.
 *   E key near docking ring → Disembark() → controller returns to character.
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaShipPawn : public APawn
{
    GENERATED_BODY()

public:
    ATartariaShipPawn();

protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;

public:
    virtual void Tick(float DeltaTime) override;

    // -------------------------------------------------------
    // Ship mesh — compound primitives from Engine/BasicShapes
    // -------------------------------------------------------

    /** Primary cylinder hull — scaled tall and wide. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Mesh")
    UStaticMeshComponent* ShipBody;

    /** Cone nose at the forward (+X) tip. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Mesh")
    UStaticMeshComponent* NoseCone;

    /** Port (left) wing — flat cube plate. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Mesh")
    UStaticMeshComponent* WingPortPlate;

    /** Starboard (right) wing — flat cube plate. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Mesh")
    UStaticMeshComponent* WingStarboardPlate;

    /** Blue-white engine glow behind the ship. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Mesh")
    UPointLightComponent* EngineThrustLight;

    // -------------------------------------------------------
    // Cameras
    // -------------------------------------------------------

    /** Boom for third-person external view (500 cm from hull). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Camera")
    USpringArmComponent* CameraBoom;

    /** Third-person follow camera (attached to CameraBoom). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Camera")
    UCameraComponent* FollowCamera;

    /** First-person cockpit camera (inside ship, looking forward). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Camera")
    UCameraComponent* CockpitCamera;

    // -------------------------------------------------------
    // Flight stats
    // -------------------------------------------------------

    UPROPERTY(BlueprintReadOnly, Category = "Ship|Flight")
    float CurrentSpeed = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Flight")
    float MaxSpeed = 2000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Flight")
    float ThrustPower = 800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship|Flight")
    float RotationSpeed = 90.0f;

    // -------------------------------------------------------
    // Resource stats (synced from Python)
    // -------------------------------------------------------

    UPROPERTY(BlueprintReadOnly, Category = "Ship|Stats")
    float CurrentFuel = 1000.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Ship|Stats")
    float MaxFuel = 1000.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Ship|Stats")
    float HullHP = 100.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Ship|Stats")
    float MaxHullHP = 100.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Ship|Stats")
    float ShieldHP = 50.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Ship|Stats")
    float MaxShieldHP = 50.0f;

    // -------------------------------------------------------
    // Flight state
    // -------------------------------------------------------

    /** Current flight state label ("docked" / "launching" / "supercruise" / "orbital"). */
    UPROPERTY(BlueprintReadOnly, Category = "Ship|Flight")
    FString FlightState = TEXT("docked");

    // -------------------------------------------------------
    // Public interface
    // -------------------------------------------------------

    /**
     * Possess this pawn, storing a reference to the pilot character.
     * Hides the character mesh while boarded.
     * @param Pilot  The ATartariaCharacter that is boarding.
     */
    UFUNCTION(BlueprintCallable, Category = "Ship")
    void Board(ATartariaCharacter* Pilot);

    /**
     * Unpossess this pawn, restoring controller to the pilot character.
     * Teleports the pilot to the ship's current location and unhides them.
     */
    UFUNCTION(BlueprintCallable, Category = "Ship")
    void Disembark();

    /**
     * Switch between cockpit (first-person) and external (third-person) camera.
     * Bound to C key.
     */
    UFUNCTION(BlueprintCallable, Category = "Ship|Camera")
    void ToggleCameraView();

    /**
     * Push updated HUD overlay data: speed, fuel percentage, heading.
     * Called every frame from Tick().
     */
    UFUNCTION(BlueprintCallable, Category = "Ship|HUD")
    void UpdateHUD();

    /**
     * Sync ship stats from a JSON response received from the Python backend
     * (called after GET /api/game/ship/status).
     */
    UFUNCTION(BlueprintCallable, Category = "Ship")
    void SyncFromBackend(const FString& JsonPayload);

    // -------------------------------------------------------
    // Delegates
    // -------------------------------------------------------

    UPROPERTY(BlueprintAssignable, Category = "Ship")
    FOnShipDamaged OnShipDamaged;

    UPROPERTY(BlueprintAssignable, Category = "Ship")
    FOnShipDocked OnShipDocked;

private:
    // ── Input handlers ───────────────────────────────────────
    void ThrustForward(float Value);
    void ThrustRight(float Value);
    void PitchInput(float Value);
    void RollInput(float Value);
    void YawInput(float Value);
    void OnInteractInput();

    // ── Internal helpers ─────────────────────────────────────

    /** Build compound mesh parts.  Called from constructor. */
    void SetupShipMesh();

    /**
     * Create and attach a ship hull sub-component.
     * @param Name      Component name for the sub-object registry.
     * @param ShapePath Relative path under Engine/BasicShapes (e.g. TEXT("Cylinder")).
     * @param Parent    Parent component (ShipBody for wing/nose, nullptr for root).
     * @param RelLoc    Location relative to parent in cm.
     * @param RelRot    Rotation relative to parent.
     * @param Scale     Non-uniform scale applied to the primitive.
     * @param Color     Emissive / diffuse tint (FLinearColor).
     */
    UStaticMeshComponent* CreateHullPart(const FName& Name, const TCHAR* ShapePath,
        USceneComponent* Parent, const FVector& RelLoc,
        const FRotator& RelRot, const FVector& Scale,
        const FLinearColor& Color);

    // ── Pilot reference ──────────────────────────────────────
    UPROPERTY()
    ATartariaCharacter* PilotCharacter = nullptr;

    // ── Camera state ─────────────────────────────────────────
    bool bCockpitView = false;

    // ── Input accumulation ───────────────────────────────────
    float ThrottleInput = 0.0f;    // +1 forward, -1 brake
    float YawRate     = 0.0f;
    float PitchRate   = 0.0f;
    float RollRate    = 0.0f;

    // ── Fuel consumption (client-side estimate, authoritative on Python) ──
    float FuelBurnTimer = 0.0f;
    float FuelSyncTimer = 0.0f;
    static constexpr float FuelSyncInterval = 5.0f;   // POST status every 5 s
};
