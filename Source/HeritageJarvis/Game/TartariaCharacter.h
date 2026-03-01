#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TartariaTypes.h"
#include "TartariaCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class ATartariaWarpEffect;
class UProceduralMeshComponent;
class UBoxComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFootstep, bool, bSprinting);

/**
 * ATartariaCharacter — Player character for the Tartaria game world.
 * Uses legacy input system (configured via DefaultInput.ini — no assets needed).
 * WASD move, mouse look, Space jump, Shift sprint, E interact, Escape menu, F3 debug.
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ATartariaCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* Input) override;

public:
	virtual void Tick(float DeltaTime) override;

	// -------------------------------------------------------
	// Camera
	// -------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	USpringArmComponent* SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* Camera;

	// -------------------------------------------------------
	// Avatar (Compound Primitive Exosuit)
	// -------------------------------------------------------

	/** Torso — primary cylinder body. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Avatar")
	UStaticMeshComponent* AvatarTorso;

	/** Head — sphere with visor. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Avatar")
	UStaticMeshComponent* AvatarHead;

	/** Visor — flattened cube across the face. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Avatar")
	UStaticMeshComponent* AvatarVisor;

	/** Left arm — thin cylinder. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Avatar")
	UStaticMeshComponent* AvatarArmL;

	/** Right arm — thin cylinder. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Avatar")
	UStaticMeshComponent* AvatarArmR;

	/** Left leg — thin cylinder. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Avatar")
	UStaticMeshComponent* AvatarLegL;

	/** Right leg — thin cylinder. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Avatar")
	UStaticMeshComponent* AvatarLegR;

	/** Shoulder pads — flattened spheres. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Avatar")
	UStaticMeshComponent* AvatarShoulderL;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Avatar")
	UStaticMeshComponent* AvatarShoulderR;

	/** Backpack reactor — small glowing cylinder. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Avatar")
	UStaticMeshComponent* AvatarBackpack;

	/** Visor glow light. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Avatar")
	UPointLightComponent* VisorLight;

	/** Build the compound primitive avatar mesh. Called from constructor. */
	void SetupAvatarMesh();

	// -------------------------------------------------------
	// Stats
	// -------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float WalkSpeed = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float SprintSpeed = 800.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	bool bIsSprinting = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact")
	float InteractReach = 250.0f;

	// -------------------------------------------------------
	// Health / Combat (Phase 2)
	// -------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Combat")
	float MaxHealth = 100.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Stats|Combat")
	float CurrentHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Combat")
	int32 Defense = 20;

	UPROPERTY(BlueprintReadOnly, Category = "Stats|Combat")
	bool bIsDead = false;

	// -------------------------------------------------------
	// Weapon System
	// -------------------------------------------------------

	/** Weapon mesh loaded from forge pipeline. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats|Combat")
	UProceduralMeshComponent* WeaponMesh = nullptr;

	/** Dynamic hitbox derived from weapon CAD geometry (Task #203).
	 *  Disabled by default, enabled only during attack hit window. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats|Combat")
	UBoxComponent* WeaponHitbox = nullptr;

	/** Combat stats derived from equipped weapon mesh geometry. */
	UPROPERTY(BlueprintReadOnly, Category = "Stats|Combat")
	FWeaponCombatStats EquippedWeaponStats;

	/** Currently equipped weapon ID (from Python backend). */
	UPROPERTY(BlueprintReadOnly, Category = "Stats|Combat")
	FString EquippedWeaponId;

	/** Equip a weapon by fetching its mesh from the backend. */
	UFUNCTION(BlueprintCallable, Category = "Stats|Combat")
	void EquipWeaponMesh(const FString& WeaponId);

	/** Clear equipped weapon mesh. */
	UFUNCTION(BlueprintCallable, Category = "Stats|Combat")
	void UnequipWeapon();

	/**
	 * Apply weapon combat stats to the character (Task #203).
	 * Configures the dynamic hitbox extents, attack speed modifier,
	 * and base damage from mesh-derived combat data.
	 */
	UFUNCTION(BlueprintCallable, Category = "Stats|Combat")
	void ApplyWeaponStats(const FWeaponCombatStats& Stats);

	/** Enable the weapon hitbox (called at start of attack hit window). */
	void EnableWeaponHitbox();

	/** Disable the weapon hitbox (called at end of attack or swing). */
	void DisableWeaponHitbox();

	/** Current biome the player is in. */
	UPROPERTY(BlueprintReadOnly, Category = "Stats|Combat")
	FString CurrentBiome = TEXT("CLEARINGHOUSE");

	UFUNCTION(BlueprintCallable, Category = "Stats|Combat")
	void ApplyDamage(float DamageAmount);

	UFUNCTION(BlueprintCallable, Category = "Stats|Combat")
	void Heal(float HealAmount);

	// -------------------------------------------------------
	// Transit / Flight State (Phase: Solar System)
	// -------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "Stats|Transit")
	EFlightState FlightState = EFlightState::OnFoot;

	UPROPERTY(BlueprintReadOnly, Category = "Stats|Transit")
	FTartariaTransitStatus TransitStatus;

	/** The celestial body the player is currently at. */
	UPROPERTY(BlueprintReadOnly, Category = "Stats|Transit")
	FString CurrentBody = TEXT("earth");

	/** Request transit to a destination body via Python backend. */
	UFUNCTION(BlueprintCallable, Category = "Stats|Transit")
	void RequestTransit(const FString& DestinationBody);

	/** Called when transit completes (timer or server confirmation). */
	void OnTransitArrived();

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFlightStateChanged, EFlightState, NewState);

	UPROPERTY(BlueprintAssignable, Category = "Stats|Transit")
	FOnFlightStateChanged OnFlightStateChanged;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTransitProgress, float, Progress, const FString&, Destination);

	UPROPERTY(BlueprintAssignable, Category = "Stats|Transit")
	FOnTransitProgress OnTransitProgress;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, NewHealth, float, MaxHealthVal);

	UPROPERTY(BlueprintAssignable, Category = "Stats|Combat")
	FOnHealthChanged OnHealthChanged;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerDeath);

	UPROPERTY(BlueprintAssignable, Category = "Stats|Combat")
	FOnPlayerDeath OnPlayerDeath;

	// -------------------------------------------------------
	// Blueprint events
	// -------------------------------------------------------

	UFUNCTION(BlueprintImplementableEvent, Category = "Tartaria")
	void OnInteract();

	UFUNCTION(BlueprintImplementableEvent, Category = "Tartaria")
	void OnMenuToggled();

	/** Toggle CEF dashboard overlay (bound to Tab key). */
	UFUNCTION(BlueprintCallable, Category = "Tartaria")
	void ToggleDashboard();

	/** Toggle inventory panel (bound to I key). */
	UFUNCTION(BlueprintCallable, Category = "Tartaria")
	void ToggleInventory();

	/** Open governance panel via CEF (bound to G key). */
	UFUNCTION(BlueprintCallable, Category = "Tartaria")
	void OpenGovernance();

	/** Open star map / transit UI (bound to M key). */
	UFUNCTION(BlueprintCallable, Category = "Tartaria")
	void OpenStarMap();

	// -------------------------------------------------------
	// Phase 1: Footstep event delegate
	// -------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "HJ|Movement")
	FOnFootstep OnFootstep;

	// ── Environmental Effects ────────────────────────────────
	/** Spawn a dust puff at the character's feet. Called during movement. */
	void SpawnFootstepDust();

	/** Timer for spacing footstep dust particles. */
	float FootstepDustTimer = 0.f;

	/** Footstep dust interval (seconds between puffs). */
	float FootstepDustInterval = 0.4f;

	// ── Material-Specific Audio (Task #205) ──────────────────

	/**
	 * Determine the physical material under the character's feet.
	 * Performs a line trace downward and maps actor tags or
	 * UPhysicalMaterial to ETartariaPhysicalMaterial.
	 * Falls back to Stone if nothing specific is found.
	 */
	UFUNCTION(BlueprintCallable, Category = "HJ|Audio")
	ETartariaPhysicalMaterial GetFloorMaterial() const;

	/**
	 * Play a material-aware footstep sound at the character's feet.
	 * Called automatically from Tick() when moving on the ground.
	 */
	void PlayMaterialFootstep();

	/**
	 * The weapon's physical material type (used for combat hit sound blending).
	 * Defaults to Metal. Updated when equipping a weapon from the backend.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HJ|Audio")
	ETartariaPhysicalMaterial WeaponMaterialType = ETartariaPhysicalMaterial::Metal;

	// -------------------------------------------------------
	// Stamina System
	// -------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Stamina")
	float MaxStamina = 100.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Stats|Stamina")
	float CurrentStamina = 100.0f;

	/** Stamina regen rate per second (after idle delay). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Stamina")
	float StaminaRegenRate = 8.0f;

	/** Seconds of no stamina use before regen starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Stamina")
	float StaminaRegenDelay = 1.5f;

	/** Stamina cost to dodge roll. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Stamina")
	float DodgeStaminaCost = 25.0f;

	/** Stamina drain per second while sprinting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Stamina")
	float SprintStaminaDrain = 10.0f;

	/** Base stamina cost for a melee attack. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Stamina")
	float AttackStaminaCost = 20.0f;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStaminaChanged, float, NewStamina, float, MaxStaminaVal);

	UPROPERTY(BlueprintAssignable, Category = "Stats|Stamina")
	FOnStaminaChanged OnStaminaChanged;

	/** Try to consume stamina. Returns true if enough was available. */
	bool ConsumeStamina(float Amount);

	// -------------------------------------------------------
	// Melee Combat
	// -------------------------------------------------------

	/** Begin a melee attack swing. */
	UFUNCTION(BlueprintCallable, Category = "HJ|Combat")
	void MeleeAttack();

	/** Is currently in an attack animation? */
	UPROPERTY(BlueprintReadOnly, Category = "HJ|Combat")
	bool bIsAttacking = false;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMeleeHit, AActor*, HitActor, float, DamageDealt);

	UPROPERTY(BlueprintAssignable, Category = "HJ|Combat")
	FOnMeleeHit OnMeleeHit;

	/** Attack input action binding. */
	void OnAttackInput();

	// -------------------------------------------------------
	// Phase 2: Combat feel
	// -------------------------------------------------------

	UFUNCTION(BlueprintCallable)
	void DodgeRoll();

	/** Phase 2: Apply screen shake from damage. */
	UFUNCTION(BlueprintCallable, Category = "HJ|Combat")
	void ApplyScreenShake(float Intensity, float Duration = 0.3f);

	/** Phase 2: Show damage direction indicator. */
	void SetDamageDirection(const FVector& SourceLocation);

	/** Landing override for camera bump. */
	virtual void Landed(const FHitResult& Hit) override;

private:
	void MoveForward(float Value);
	void MoveRight(float Value);
	void Turn(float Value);
	void LookUp(float Value);
	void StartSprint();
	void StopSprint();
	void Interact();
	void ToggleMenu();
	void ToggleDebug();
	void DoInteractTrace();
	void UpdateTransit(float DeltaTime);
	void AnimateIdle(float DeltaTime);

	/** Fetch combat stats from backend and apply to weapon hitbox (Task #203). */
	void FetchAndApplyWeaponCombatStats(const FString& WeaponId);

	/** Helper: create and attach a primitive mesh sub-component. */
	UStaticMeshComponent* CreateAvatarPart(const FName& Name, const TCHAR* ShapePath,
		const FVector& RelLoc, const FRotator& RelRot, const FVector& Scale,
		const FLinearColor& Color);

	/** Timer handle for transit completion. */
	FTimerHandle TransitTimerHandle;

	/** Active warp VFX actor (spawned during supercruise, destroyed on arrival). */
	UPROPERTY()
	ATartariaWarpEffect* ActiveWarpEffect = nullptr;

	/** Spawn warp VFX attached to player. */
	void SpawnWarpEffect(const FString& DestinationBody);

	/** Destroy warp VFX. */
	void DestroyWarpEffect();

	// Dodge roll
	bool bIsDodging = false;
	float DodgeRollTimer = 0.0f;
	float DodgeRollDuration = 0.6f;
	float DodgeRollCooldown = 1.5f;
	float DodgeRollCooldownTimer = 0.0f;
	FVector DodgeDirection;
	float DodgeRollSpeed = 1200.0f;

	// Camera breathing (extends idle to all states)
	float BreathPhase = 0.0f;
	float BreathAmplitude = 0.3f;  // subtle Z offset in cm
	float BreathRate = 0.4f;       // Hz - slow, organic breathing

	/** Accumulated time for idle breathing animation. */
	float IdleAnimTime = 0.f;

	// ── Phase 1: Movement Grounding ──────────────────────────────
	/** Timer for footstep sound events. Interval varies by movement mode. */
	float FootstepTimer = 0.f;

	/** Cached floor material from last footstep trace. */
	ETartariaPhysicalMaterial CachedFloorMaterial = ETartariaPhysicalMaterial::Stone;

	/** Timer to throttle floor material traces (re-check every 0.3s). */
	float FloorMaterialCheckTimer = 0.f;

	/** Camera landing recovery (bumps down on land, recovers over 0.2s). */
	float LandingRecoveryTimer = 0.f;
	float LandingBumpOffset = 0.f;

	/** Head bob phase accumulator for walk/sprint sway. */
	float HeadBobPhase = 0.f;

	/** Target sprint FOV (lerps 90→95 on sprint). */
	float CurrentFOV = 90.f;
	float TargetFOV = 90.f;

	// ── Phase 2: Combat Feel ─────────────────────────────────────
	/** Screen shake remaining duration. */
	float ScreenShakeTimer = 0.f;
	float ScreenShakeIntensity = 0.f;

	/** Red vignette flash intensity (fades over 0.4s). */
	float DamageVignetteAlpha = 0.f;

	/** Direction of last damage source (for hit indicator). */
	FVector LastDamageDirection = FVector::ZeroVector;
	float DamageDirectionTimer = 0.f;

	// ── Melee Attack State ──────────────────────────────────────
	float AttackTimer = 0.f;
	float AttackDuration = 0.5f;          // total swing time
	float AttackHitWindow = 0.25f;         // time within swing when hit detection is active
	bool bAttackHitChecked = false;        // prevent multi-hit per swing
	float AttackCooldown = 0.3f;           // time between attacks
	float AttackCooldownTimer = 0.f;

	// Hitstop
	float HitstopTimer = 0.f;
	float HitstopDuration = 0.06f;
	bool bInHitstop = false;

	// Stamina regen timer
	float StaminaIdleTimer = 0.f;

	// Attack slow (when stamina depleted)
	float AttackSpeedMult = 1.0f;

	// Weapon stats attack speed modifier (from CAD geometry, Task #203)
	float WeaponSpeedMult = 1.0f;
};
