#include "TartariaCharacter.h"
#include "TartariaGameMode.h"
#include "TartariaEnemyActor.h"
#include "TartariaSolarSystemManager.h"
#include "TartariaWarpEffect.h"
#include "TartariaLevelStreamManager.h"
#include "HJInteractable.h"
#include "UI/HJNotificationWidget.h"
#include "UI/HJDialogueWidget.h"
#include "Core/HJGameInstance.h"
#include "Core/HJApiClient.h"
#include "Core/HJMeshLoader.h"
#include "Core/TartariaSoundManager.h"
#include "ProceduralMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

ATartariaCharacter::ATartariaCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Spring arm — 3rd person offset
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength         = 400.0f;
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->bEnableCameraLag        = true;
	SpringArm->CameraLagSpeed          = 8.0f;
	SpringArm->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));

	// Camera
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	// Movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->JumpZVelocity             = 600.0f;
	GetCharacterMovement()->MaxWalkSpeed              = WalkSpeed;

	// ── Phase 1: Grounding ───────────────────────────────────────
	// CHANGED from defaults: reduce air control and increase gravity for weight
	GetCharacterMovement()->AirControl                = 0.08f;      // was 0.35
	GetCharacterMovement()->GravityScale              = 1.4f;       // was 1.0
	GetCharacterMovement()->RotationRate              = FRotator(0.f, 280.f, 0.f);  // was 540
	// Add ground friction for momentum feel
	GetCharacterMovement()->GroundFriction            = 6.0f;       // was 8.0 (default)
	GetCharacterMovement()->BrakingDecelerationWalking = 1200.f;    // was 2048 — slower stop

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw   = false;
	bUseControllerRotationRoll  = false;

	// Build the compound primitive "Tartarian Exosuit" avatar
	SetupAvatarMesh();

	// Weapon attachment point on right hand
	WeaponMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(AvatarArmR);
	WeaponMesh->SetRelativeLocation(FVector(0.f, 0.f, -50.f));  // Below arm tip
	WeaponMesh->SetRelativeScale3D(FVector(0.1f, 0.1f, 0.1f));  // mm to cm scaling handled by loader
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetVisibility(false);  // Hidden until weapon equipped

	// Dynamic weapon hitbox (Task #203) — attached to right arm, sized from CAD data
	WeaponHitbox = CreateDefaultSubobject<UBoxComponent>(TEXT("WeaponHitbox"));
	WeaponHitbox->SetupAttachment(AvatarArmR);
	WeaponHitbox->SetRelativeLocation(FVector(0.f, 0.f, -50.f));  // Same anchor as weapon mesh
	WeaponHitbox->SetBoxExtent(FVector(5.f, 5.f, 25.f));  // Default extents (overridden by ApplyWeaponStats)
	WeaponHitbox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	WeaponHitbox->SetGenerateOverlapEvents(true);
	WeaponHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);  // Disabled until attack
	WeaponHitbox->SetHiddenInGame(true);
#if WITH_EDITORONLY_DATA
	WeaponHitbox->ShapeColor = FColor::Red;
	WeaponHitbox->SetLineThickness(2.0f);
#endif
}

void ATartariaCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void ATartariaCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
	Super::SetupPlayerInputComponent(Input);

	// Legacy input — bindings come from DefaultInput.ini, no assets needed
	Input->BindAxis("MoveForward", this, &ATartariaCharacter::MoveForward);
	Input->BindAxis("MoveRight",   this, &ATartariaCharacter::MoveRight);
	Input->BindAxis("Turn",        this, &ATartariaCharacter::Turn);
	Input->BindAxis("LookUp",      this, &ATartariaCharacter::LookUp);

	Input->BindAction("Jump",      IE_Pressed,  this, &ATartariaCharacter::Jump);
	Input->BindAction("Jump",      IE_Released, this, &ATartariaCharacter::StopJumping);
	Input->BindAction("Sprint",    IE_Pressed,  this, &ATartariaCharacter::StartSprint);
	Input->BindAction("Sprint",    IE_Released, this, &ATartariaCharacter::StopSprint);
	Input->BindAction("Interact",  IE_Pressed,  this, &ATartariaCharacter::Interact);
	Input->BindAction("OpenMenu",  IE_Pressed,  this, &ATartariaCharacter::ToggleMenu);
	Input->BindAction("DebugToggle", IE_Pressed, this, &ATartariaCharacter::ToggleDebug);
	// Dodge roll on Alt or double-tap direction
	Input->BindAction("Dodge", IE_Pressed, this, &ATartariaCharacter::DodgeRoll);
	Input->BindAction("DashboardToggle", IE_Pressed, this, &ATartariaCharacter::ToggleDashboard);
	Input->BindAction("InventoryToggle", IE_Pressed, this, &ATartariaCharacter::ToggleInventory);
	Input->BindAction("GovernanceToggle", IE_Pressed, this, &ATartariaCharacter::OpenGovernance);
	Input->BindAction("StarMapToggle", IE_Pressed, this, &ATartariaCharacter::OpenStarMap);
	Input->BindAction("Attack", IE_Pressed, this, &ATartariaCharacter::OnAttackInput);
}

void ATartariaCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (FlightState == EFlightState::Supercruise)
		UpdateTransit(DeltaTime);

	// Idle breathing/sway animation
	AnimateIdle(DeltaTime);

	// ── Phase 1+2: Movement Grounding & Combat Feel ──────────────
	float DT = DeltaTime;

	// ── Footstep Timer (Material-Aware, Task #205) ──────────────
	bool bMoving = GetVelocity().Size2D() > 10.f;
	bool bOnGround = GetCharacterMovement()->IsMovingOnGround();
	if (bMoving && bOnGround)
	{
		// Determine footstep interval based on movement mode.
		// Walking=0.5s, Sprinting=0.35s, Crouching=0.7s.
		// Add slight random variation (+/-10%) to prevent mechanical feel.
		float BaseInterval;
		if (GetCharacterMovement()->IsCrouching())
			BaseInterval = 0.7f;
		else if (bIsSprinting)
			BaseInterval = 0.35f;
		else
			BaseInterval = 0.5f;

		float StepInterval = BaseInterval * FMath::RandRange(0.9f, 1.1f);

		FootstepTimer += DT;
		if (FootstepTimer >= StepInterval)
		{
			FootstepTimer = 0.f;

			// Refresh floor material periodically (throttled)
			FloorMaterialCheckTimer += StepInterval;
			if (FloorMaterialCheckTimer >= 0.3f)
			{
				CachedFloorMaterial = GetFloorMaterial();
				FloorMaterialCheckTimer = 0.f;
			}

			// Broadcast footstep event (Blueprint hook)
			OnFootstep.Broadcast(bIsSprinting);

			// Play material-aware footstep sound
			PlayMaterialFootstep();
		}
	}
	else
	{
		FootstepTimer = 0.f;
	}

	// ── Footstep Dust ─────────────────────────────────────
	if (GetVelocity().Size() > 50.f && !GetCharacterMovement()->IsFalling())
	{
		FootstepDustTimer += DT;
		float DustInterval = GetCharacterMovement()->IsCrouching() ? FootstepDustInterval * 1.5f :
		                     (bIsSprinting ? FootstepDustInterval * 0.6f : FootstepDustInterval);
		if (FootstepDustTimer >= DustInterval)
		{
			FootstepDustTimer = 0.f;
			SpawnFootstepDust();
		}
	}
	else
	{
		FootstepDustTimer = 0.f;
	}

	// ── Sprint FOV ───────────────────────────────────────────────
	if (!bIsDodging)
		TargetFOV = bIsSprinting ? 95.f : 90.f;
	CurrentFOV = FMath::FInterpTo(CurrentFOV, TargetFOV, DT, 6.f);
	if (SpringArm && SpringArm->GetChildComponent(0))
	{
		UCameraComponent* Cam = Cast<UCameraComponent>(SpringArm->GetChildComponent(0));
		if (Cam) Cam->SetFieldOfView(CurrentFOV);
	}

	// ── Dodge Roll Animation ────────────────────────────────────
	if (bIsDodging)
	{
		DodgeRollTimer += DT;
		float RollAlpha = DodgeRollTimer / DodgeRollDuration;

		if (RollAlpha < 1.0f)
		{
			// Launch character along dodge direction
			float SpeedCurve = FMath::Sin(RollAlpha * PI);  // peak in middle
			FVector DodgeVelocity = DodgeDirection * DodgeRollSpeed * SpeedCurve;
			GetCharacterMovement()->Velocity = FVector(DodgeVelocity.X, DodgeVelocity.Y, GetCharacterMovement()->Velocity.Z);

			// Camera dip during roll
			float RollDip = FMath::Sin(RollAlpha * PI) * -15.0f;
			if (SpringArm)
			{
				SpringArm->SocketOffset.Z += RollDip * DT * 10.0f;
			}
		}
		else
		{
			bIsDodging = false;
			DodgeRollCooldownTimer = DodgeRollCooldown;
			TargetFOV = bIsSprinting ? 95.0f : 90.0f;
		}
	}

	// ── Dodge Cooldown ──────────────────────────────────────────
	if (DodgeRollCooldownTimer > 0.0f)
	{
		DodgeRollCooldownTimer -= DT;
	}

	// ── Head Bob ─────────────────────────────────────────────────
	if (bMoving && bOnGround)
	{
		float BobSpeed = bIsSprinting ? 12.f : 8.f;
		float BobAmplitude = bIsSprinting ? 3.f : 1.5f;  // cm
		HeadBobPhase += DT * BobSpeed;
		float BobOffset = FMath::Sin(HeadBobPhase) * BobAmplitude;
		// Apply to spring arm relative location Z offset
		if (SpringArm)
		{
			FVector Loc = SpringArm->GetRelativeLocation();
			Loc.Z = 60.f + BobOffset + LandingBumpOffset;
			SpringArm->SetRelativeLocation(Loc);
		}
	}
	else
	{
		// Decay head bob
		HeadBobPhase = 0.f;
		if (SpringArm)
		{
			FVector Loc = SpringArm->GetRelativeLocation();
			Loc.Z = FMath::FInterpTo(Loc.Z, 60.f + LandingBumpOffset, DT, 10.f);
			SpringArm->SetRelativeLocation(Loc);
		}
	}

	// ── Subtle Camera Breathing (all states) ────────────────────
	BreathPhase += DT * BreathRate * 2.0f * PI;
	if (BreathPhase > 2.0f * PI) BreathPhase -= 2.0f * PI;
	float BreathOffset = FMath::Sin(BreathPhase) * BreathAmplitude;
	if (SpringArm)
	{
		// Breathing adds a very subtle Z oscillation
		SpringArm->SocketOffset.Z += BreathOffset;

		// Slight FOV breathing when idle (not sprinting/dodging)
		if (!bIsSprinting && !bIsDodging && GetVelocity().SizeSquared() < 100.0f)
		{
			CurrentFOV += FMath::Sin(BreathPhase * 0.5f) * 0.15f;  // +/-0.15 degree subtle
		}
	}

	// ── Landing Recovery ─────────────────────────────────────────
	if (LandingRecoveryTimer > 0.f)
	{
		LandingRecoveryTimer -= DT;
		LandingBumpOffset = FMath::FInterpTo(LandingBumpOffset, 0.f, DT, 8.f);
		if (LandingRecoveryTimer <= 0.f)
		{
			LandingBumpOffset = 0.f;
		}
	}

	// ── Screen Shake ─────────────────────────────────────────────
	if (ScreenShakeTimer > 0.f)
	{
		ScreenShakeTimer -= DT;
		float ShakeMag = ScreenShakeIntensity * (ScreenShakeTimer / 0.3f);
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (PC && PC->PlayerCameraManager)
		{
			FVector ShakeOffset(
				FMath::RandRange(-ShakeMag, ShakeMag),
				FMath::RandRange(-ShakeMag, ShakeMag),
				0.f
			);
			PC->PlayerCameraManager->ViewTarget.POV.Location += ShakeOffset;
		}
		if (ScreenShakeTimer <= 0.f)
		{
			ScreenShakeIntensity = 0.f;
		}
	}

	// ── Damage Vignette Fade ─────────────────────────────────────
	if (DamageVignetteAlpha > 0.f)
	{
		DamageVignetteAlpha = FMath::FInterpTo(DamageVignetteAlpha, 0.f, DT, 3.f);
	}

	// ── Damage Direction Indicator Fade ──────────────────────────
	if (DamageDirectionTimer > 0.f)
	{
		DamageDirectionTimer -= DT;
	}

	// ── Stamina System ──────────────────────────────────────────
	StaminaIdleTimer += DT;

	// Sprint stamina drain
	if (bIsSprinting && GetVelocity().Size() > 10.f)
	{
		if (!ConsumeStamina(SprintStaminaDrain * DT))
		{
			// Out of stamina - force stop sprint
			StopSprint();
		}
	}

	// Stamina regen (after idle delay)
	if (StaminaIdleTimer >= StaminaRegenDelay && CurrentStamina < MaxStamina)
	{
		CurrentStamina = FMath::Min(MaxStamina, CurrentStamina + StaminaRegenRate * DT);
		OnStaminaChanged.Broadcast(CurrentStamina, MaxStamina);
	}

	// Attack speed penalty at low stamina
	AttackSpeedMult = (CurrentStamina <= 0.f) ? 0.5f : 1.0f;

	// ── Melee Attack Animation ──────────────────────────────────
	if (bIsAttacking)
	{
		// Apply both stamina-based and weapon-weight-based speed modifiers
		AttackTimer += DT * AttackSpeedMult * WeaponSpeedMult;

		// Swing the weapon mesh (or right arm if no weapon)
		float SwingPhase = AttackTimer / AttackDuration;
		if (WeaponMesh && WeaponMesh->IsVisible())
		{
			// Rotate weapon in an arc: -45 to +90 degrees over the swing
			float SwingAngle = FMath::Lerp(-45.f, 90.f, FMath::Clamp(SwingPhase * 2.f, 0.f, 1.f));
			WeaponMesh->SetRelativeRotation(FRotator(SwingAngle, 0.f, 0.f));
		}
		else if (AvatarArmR)
		{
			// Arm punch if no weapon
			float PunchAngle = FMath::Lerp(0.f, -60.f, FMath::Clamp(SwingPhase * 2.f, 0.f, 1.f));
			float PunchReturn = SwingPhase > 0.5f ? FMath::Lerp(-60.f, 0.f, (SwingPhase - 0.5f) * 2.f) : PunchAngle;
			AvatarArmR->SetRelativeRotation(FRotator(PunchReturn, 0.f, 0.f));
		}

		// Enable weapon hitbox during the active hit window (Task #203)
		if (SwingPhase >= 0.2f && SwingPhase <= 0.6f)
		{
			EnableWeaponHitbox();
		}
		else
		{
			DisableWeaponHitbox();
		}

		// Hit detection window — use weapon stats for reach and damage
		if (SwingPhase >= 0.2f && SwingPhase <= 0.6f && !bAttackHitChecked)
		{
			// Use weapon reach from combat stats if available, otherwise default
			float WeaponReach = EquippedWeaponStats.bIsValid ? EquippedWeaponStats.Reach : 200.f;

			// Sphere sweep in front of player using weapon reach
			FVector TraceStart = GetActorLocation() + GetActorForwardVector() * 50.f;
			FVector TraceEnd = TraceStart + GetActorForwardVector() * WeaponReach;
			float TraceRadius = 60.f;

			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(this);

			TArray<FHitResult> Hits;
			bool bHit = GetWorld()->SweepMultiByChannel(
				Hits, TraceStart, TraceEnd, FQuat::Identity,
				ECollisionChannel::ECC_Pawn, FCollisionShape::MakeSphere(TraceRadius),
				QueryParams);

			if (bHit)
			{
				bAttackHitChecked = true;

				for (const FHitResult& Hit : Hits)
				{
					AActor* HitActor = Hit.GetActor();
					if (!HitActor) continue;

					// Use weapon-derived damage if valid, otherwise default (Task #203)
					float BaseDamage = EquippedWeaponStats.bIsValid
						? static_cast<float>(EquippedWeaponStats.BaseDamage)
						: 15.f;

					// Determine target material from actor tags for
					// material-specific combat hit sounds (Task #205).
					// Default to Stone for generic enemies.
					ETartariaPhysicalMaterial TargetMat = ETartariaPhysicalMaterial::Stone;
					if (HitActor->Tags.Contains(TEXT("Metal")))
						TargetMat = ETartariaPhysicalMaterial::Metal;
					else if (HitActor->Tags.Contains(TEXT("Wood")))
						TargetMat = ETartariaPhysicalMaterial::Wood;
					else if (HitActor->Tags.Contains(TEXT("Crystal")))
						TargetMat = ETartariaPhysicalMaterial::Crystal;
					else if (HitActor->Tags.Contains(TEXT("Earth")))
						TargetMat = ETartariaPhysicalMaterial::Earth;

					// Play material-aware combat hit (weapon material vs target material)
					UTartariaSoundManager::PlayMaterialCombatHit(
						this, WeaponMaterialType, TargetMat,
						Hit.ImpactPoint, BaseDamage);

					// Apply damage to enemy actors
					ATartariaEnemyActor* Enemy = Cast<ATartariaEnemyActor>(HitActor);
					if (Enemy && !Enemy->bDefeated)
					{
						// Trigger combat choreography - player wins if we hit them
						Enemy->BeginCombatChoreography(true);

						// Knockback — scale with weapon weight (Task #203)
						float KnockbackForce = EquippedWeaponStats.bIsValid
							? 400.f + EquippedWeaponStats.WeightKg * 50.f
							: 600.f;
						FVector KnockDir = (HitActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
						ACharacter* HitChar = Cast<ACharacter>(HitActor);
						if (HitChar)
						{
							HitChar->LaunchCharacter(KnockDir * KnockbackForce + FVector(0, 0, 200.f), true, true);
						}

						// Hitstop
						bInHitstop = true;
						HitstopTimer = 0.f;
						if (UGameplayStatics::GetGlobalTimeDilation(this) > 0.5f)
						{
							UGameplayStatics::SetGlobalTimeDilation(this, 0.1f);
						}

						// Camera shake — heavier weapons shake more
						float ShakeIntensity = EquippedWeaponStats.bIsValid
							? FMath::Clamp(0.5f + EquippedWeaponStats.WeightKg * 0.1f, 0.5f, 1.5f)
							: 0.8f;
						ApplyScreenShake(ShakeIntensity, 0.2f);

						OnMeleeHit.Broadcast(HitActor, BaseDamage);

						UE_LOG(LogTemp, Log, TEXT("TartariaCharacter: Melee hit on %s for %.1f damage (reach=%.0f, weapon=%d, target=%d)"),
							*HitActor->GetName(), BaseDamage, WeaponReach,
							(int32)WeaponMaterialType, (int32)TargetMat);
					}
				}
			}
		}

		// Hitstop recovery
		if (bInHitstop)
		{
			HitstopTimer += GetWorld()->GetDeltaSeconds();  // use real delta, not dilated
			if (HitstopTimer >= HitstopDuration)
			{
				bInHitstop = false;
				UGameplayStatics::SetGlobalTimeDilation(this, 1.0f);
			}
		}

		// End attack
		if (AttackTimer >= AttackDuration)
		{
			bIsAttacking = false;
			AttackCooldownTimer = AttackCooldown;
			DisableWeaponHitbox();  // Ensure hitbox is off after attack ends

			// Reset weapon/arm rotation
			if (WeaponMesh)
				WeaponMesh->SetRelativeRotation(FRotator::ZeroRotator);
			if (AvatarArmR)
				AvatarArmR->SetRelativeRotation(FRotator::ZeroRotator);
		}
	}

	// Attack cooldown
	if (AttackCooldownTimer > 0.f)
	{
		AttackCooldownTimer -= DT;
	}
}

// -------------------------------------------------------
// Avatar — Compound Primitive "Tartarian Exosuit"
// -------------------------------------------------------

UStaticMeshComponent* ATartariaCharacter::CreateAvatarPart(const FName& Name,
	const TCHAR* ShapePath, const FVector& RelLoc, const FRotator& RelRot,
	const FVector& Scale, const FLinearColor& Color)
{
	UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Part->SetupAttachment(GetMesh() ? static_cast<USceneComponent*>(GetMesh()) : RootComponent);
	Part->SetRelativeLocation(RelLoc);
	Part->SetRelativeRotation(RelRot);
	Part->SetRelativeScale3D(Scale);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->CastShadow = true;

	// Load engine primitive shape
	UStaticMesh* Shape = LoadObject<UStaticMesh>(nullptr, ShapePath);
	if (Shape)
	{
		Part->SetStaticMesh(Shape);
		UMaterialInstanceDynamic* DynMat = Part->CreateAndSetMaterialInstanceDynamic(0);
		if (DynMat)
		{
			DynMat->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}
	return Part;
}

void ATartariaCharacter::SetupAvatarMesh()
{
	/*
	 * Tartarian Exosuit — compound primitive humanoid silhouette.
	 *
	 * All dimensions in cm (UE units). Character capsule is ~88 tall by default.
	 * We build the avatar from Engine/BasicShapes primitives:
	 *   - Torso: cylinder (main body)
	 *   - Head: sphere with visor box
	 *   - Arms: thin cylinders
	 *   - Legs: thin cylinders
	 *   - Shoulders: flattened spheres (pauldrons)
	 *   - Backpack: small cylinder (reactor unit)
	 *   - Visor glow: point light
	 *
	 * Color scheme: dark bronze body, gold trim, cyan visor glow.
	 */

	const FLinearColor BodyColor(0.25f, 0.18f, 0.12f);     // Dark bronze
	const FLinearColor TrimColor(0.75f, 0.6f, 0.25f);       // Gold trim
	const FLinearColor VisorColor(0.1f, 0.7f, 0.9f);        // Cyan visor
	const FLinearColor LegColor(0.2f, 0.15f, 0.1f);         // Darker bronze

	const TCHAR* CylPath  = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	const TCHAR* SpherePath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	const TCHAR* CubePath = TEXT("/Engine/BasicShapes/Cube.Cube");

	// ── Torso: main body cylinder ────────────────────────────
	AvatarTorso = CreateAvatarPart(FName("AvatarTorso"), CylPath,
		FVector(0.f, 0.f, 10.f), FRotator::ZeroRotator,
		FVector(0.35f, 0.25f, 0.40f), BodyColor);

	// ── Head: sphere above torso ─────────────────────────────
	AvatarHead = CreateAvatarPart(FName("AvatarHead"), SpherePath,
		FVector(0.f, 0.f, 55.f), FRotator::ZeroRotator,
		FVector(0.28f, 0.28f, 0.30f), BodyColor);

	// ── Visor: flattened cube across face ────────────────────
	AvatarVisor = CreateAvatarPart(FName("AvatarVisor"), CubePath,
		FVector(8.f, 0.f, 54.f), FRotator::ZeroRotator,
		FVector(0.04f, 0.22f, 0.08f), VisorColor);
	// Make visor emissive
	if (AvatarVisor)
	{
		UMaterialInstanceDynamic* VisorMat = AvatarVisor->CreateAndSetMaterialInstanceDynamic(0);
		if (VisorMat)
		{
			VisorMat->SetVectorParameterValue(TEXT("Color"), VisorColor);
			VisorMat->SetScalarParameterValue(TEXT("Emissive"), 4.0f);
		}
	}

	// ── Visor glow light ─────────────────────────────────────
	VisorLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("VisorLight"));
	VisorLight->SetupAttachment(GetMesh() ? static_cast<USceneComponent*>(GetMesh()) : RootComponent);
	VisorLight->SetRelativeLocation(FVector(14.f, 0.f, 54.f));
	VisorLight->SetLightColor(FLinearColor(0.1f, 0.7f, 0.9f));
	VisorLight->SetIntensity(3000.f);
	VisorLight->SetAttenuationRadius(120.f);
	VisorLight->CastShadows = false;

	// ── Left arm: thin cylinder ──────────────────────────────
	AvatarArmL = CreateAvatarPart(FName("AvatarArmL"), CylPath,
		FVector(0.f, -20.f, 15.f), FRotator(0.f, 0.f, -8.f),
		FVector(0.10f, 0.10f, 0.35f), BodyColor);

	// ── Right arm: thin cylinder ─────────────────────────────
	AvatarArmR = CreateAvatarPart(FName("AvatarArmR"), CylPath,
		FVector(0.f, 20.f, 15.f), FRotator(0.f, 0.f, 8.f),
		FVector(0.10f, 0.10f, 0.35f), BodyColor);

	// ── Left leg: thin cylinder ──────────────────────────────
	AvatarLegL = CreateAvatarPart(FName("AvatarLegL"), CylPath,
		FVector(0.f, -8.f, -35.f), FRotator::ZeroRotator,
		FVector(0.12f, 0.12f, 0.35f), LegColor);

	// ── Right leg: thin cylinder ─────────────────────────────
	AvatarLegR = CreateAvatarPart(FName("AvatarLegR"), CylPath,
		FVector(0.f, 8.f, -35.f), FRotator::ZeroRotator,
		FVector(0.12f, 0.12f, 0.35f), LegColor);

	// ── Left shoulder pauldron: flattened sphere ─────────────
	AvatarShoulderL = CreateAvatarPart(FName("AvatarShoulderL"), SpherePath,
		FVector(0.f, -18.f, 38.f), FRotator::ZeroRotator,
		FVector(0.15f, 0.18f, 0.10f), TrimColor);

	// ── Right shoulder pauldron: flattened sphere ────────────
	AvatarShoulderR = CreateAvatarPart(FName("AvatarShoulderR"), SpherePath,
		FVector(0.f, 18.f, 38.f), FRotator::ZeroRotator,
		FVector(0.15f, 0.18f, 0.10f), TrimColor);

	// ── Backpack reactor: small cylinder on back ─────────────
	AvatarBackpack = CreateAvatarPart(FName("AvatarBackpack"), CylPath,
		FVector(-12.f, 0.f, 15.f), FRotator::ZeroRotator,
		FVector(0.14f, 0.14f, 0.18f), TrimColor);

	UE_LOG(LogTemp, Log, TEXT("TartariaCharacter: Exosuit avatar assembled (11 parts)"));
}

void ATartariaCharacter::AnimateIdle(float DeltaTime)
{
	IdleAnimTime += DeltaTime;

	// Gentle breathing: torso bobs up/down 1cm at 0.5 Hz
	float Breath = FMath::Sin(IdleAnimTime * 3.14f) * 1.0f;

	if (AvatarTorso)
	{
		FVector Loc = AvatarTorso->GetRelativeLocation();
		AvatarTorso->SetRelativeLocation(FVector(Loc.X, Loc.Y, 10.f + Breath));
	}

	// Head gentle tilt: +-2 degrees
	if (AvatarHead)
	{
		float HeadTilt = FMath::Sin(IdleAnimTime * 2.0f) * 2.0f;
		AvatarHead->SetRelativeRotation(FRotator(HeadTilt, 0.f, 0.f));
	}

	// Arm sway: slight pendulum
	float ArmSwing = FMath::Sin(IdleAnimTime * 2.5f) * 4.0f;
	if (AvatarArmL)
		AvatarArmL->SetRelativeRotation(FRotator(0.f, 0.f, -8.f + ArmSwing));
	if (AvatarArmR)
		AvatarArmR->SetRelativeRotation(FRotator(0.f, 0.f, 8.f - ArmSwing));

	// Visor glow pulse: subtle intensity variation
	if (VisorLight)
	{
		float GlowPulse = 2500.f + FMath::Sin(IdleAnimTime * 4.0f) * 500.f;
		VisorLight->SetIntensity(GlowPulse);
	}
}

// -------------------------------------------------------
// Environmental Effects — Footstep Dust
// -------------------------------------------------------

void ATartariaCharacter::SpawnFootstepDust()
{
	UWorld* World = GetWorld();
	if (!World) return;

	FVector FootLoc = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

	// Spawn a temporary actor to represent the dust puff
	AActor* DustActor = World->SpawnActor<AActor>(AActor::StaticClass(), FootLoc);
	if (!DustActor) return;

	// Use a static mesh sphere scaled small + flattened for dust effect
	UStaticMeshComponent* DustMesh = NewObject<UStaticMeshComponent>(DustActor);
	DustMesh->RegisterComponent();
	DustActor->SetRootComponent(DustMesh);

	UStaticMesh* SphereSM = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere"));
	if (SphereSM) DustMesh->SetStaticMesh(SphereSM);

	DustMesh->SetWorldScale3D(FVector(0.15f, 0.15f, 0.08f));
	DustMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	UMaterialInstanceDynamic* DynMat = DustMesh->CreateAndSetMaterialInstanceDynamic(0);
	if (DynMat)
	{
		DynMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.4f, 0.35f, 0.25f, 0.5f));
		DynMat->SetScalarParameterValue(TEXT("Opacity"), 0.4f);
	}

	// Auto-destroy after 0.8 seconds
	DustActor->SetLifeSpan(0.8f);

	// Scale up briefly then fade (handled by lifespan)
	TWeakObjectPtr<AActor> WeakDust(DustActor);
	FTimerHandle FadeTimer;
	World->GetTimerManager().SetTimer(FadeTimer, [WeakDust]()
	{
		AActor* D = WeakDust.Get();
		if (D)
		{
			FVector Scale = D->GetActorScale3D();
			D->SetActorScale3D(Scale * 1.3f);  // Expand
		}
	}, 0.2f, true);
}

// -------------------------------------------------------
// Material-Specific Audio (Task #205)
// -------------------------------------------------------

ETartariaPhysicalMaterial ATartariaCharacter::GetFloorMaterial() const
{
	UWorld* World = GetWorld();
	if (!World) return ETartariaPhysicalMaterial::Stone;

	// Line trace downward from the character's feet (50 units below capsule base)
	FVector TraceStart = GetActorLocation();
	FVector TraceEnd = TraceStart - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 50.f);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.bReturnPhysicalMaterial = true;

	FHitResult Hit;
	bool bHitSomething = World->LineTraceSingleByChannel(
		Hit, TraceStart, TraceEnd,
		ECollisionChannel::ECC_Visibility, QueryParams);

	if (!bHitSomething) return ETartariaPhysicalMaterial::Stone;

	// ── Strategy 1: Check actor tags ─────────────────────────
	// Actors (buildings, terrain chunks) can be tagged with material names
	// to override the default mapping.
	AActor* HitActor = Hit.GetActor();
	if (HitActor)
	{
		if (HitActor->Tags.Contains(TEXT("Metal")))
			return ETartariaPhysicalMaterial::Metal;
		if (HitActor->Tags.Contains(TEXT("Wood")))
			return ETartariaPhysicalMaterial::Wood;
		if (HitActor->Tags.Contains(TEXT("Crystal")))
			return ETartariaPhysicalMaterial::Crystal;
		if (HitActor->Tags.Contains(TEXT("Earth")))
			return ETartariaPhysicalMaterial::Earth;
		if (HitActor->Tags.Contains(TEXT("Water")))
			return ETartariaPhysicalMaterial::Water;
		if (HitActor->Tags.Contains(TEXT("Stone")))
			return ETartariaPhysicalMaterial::Stone;
	}

	// ── Strategy 2: Check UPhysicalMaterial on the surface ───
	// UE5 physical materials can be assigned in the physics asset.
	// We map the material's friction to a rough surface classification.
	UPhysicalMaterial* PhysMat = Hit.PhysMaterial.Get();
	if (PhysMat)
	{
		FString MatName = PhysMat->GetName().ToLower();
		if (MatName.Contains(TEXT("metal")) || MatName.Contains(TEXT("iron")))
			return ETartariaPhysicalMaterial::Metal;
		if (MatName.Contains(TEXT("wood")) || MatName.Contains(TEXT("plank")))
			return ETartariaPhysicalMaterial::Wood;
		if (MatName.Contains(TEXT("crystal")) || MatName.Contains(TEXT("glass")))
			return ETartariaPhysicalMaterial::Crystal;
		if (MatName.Contains(TEXT("earth")) || MatName.Contains(TEXT("dirt")) || MatName.Contains(TEXT("mud")) || MatName.Contains(TEXT("grass")))
			return ETartariaPhysicalMaterial::Earth;
		if (MatName.Contains(TEXT("water")) || MatName.Contains(TEXT("liquid")))
			return ETartariaPhysicalMaterial::Water;
	}

	// ── Strategy 3: Map by biome (fallback) ──────────────────
	// Different biome zones have characteristic ground materials.
	if (CurrentBiome == TEXT("FORGE_DISTRICT"))
		return ETartariaPhysicalMaterial::Metal;
	if (CurrentBiome == TEXT("SCRIPTORIUM"))
		return ETartariaPhysicalMaterial::Wood;
	if (CurrentBiome == TEXT("MONOLITH_WARD"))
		return ETartariaPhysicalMaterial::Crystal;
	if (CurrentBiome == TEXT("VOID_REACH"))
		return ETartariaPhysicalMaterial::Earth;

	// Default: stone is the most common Tartarian surface
	return ETartariaPhysicalMaterial::Stone;
}

void ATartariaCharacter::PlayMaterialFootstep()
{
	// Compute foot location (bottom of capsule)
	FVector FootLoc = GetActorLocation() -
		FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

	// Speed fraction: how fast the player is moving relative to max sprint speed
	float CurrentSpeed = GetVelocity().Size2D();
	float SpeedFraction = FMath::Clamp(CurrentSpeed / SprintSpeed, 0.f, 1.f);

	UTartariaSoundManager::PlayMaterialFootstep(this, CachedFloorMaterial, FootLoc, SpeedFraction);
}

// -------------------------------------------------------
// Movement
// -------------------------------------------------------

void ATartariaCharacter::MoveForward(float Value)
{
	if (Controller && Value != 0.0f)
	{
		const FRotator YawOnly(0.0f, GetControlRotation().Yaw, 0.0f);
		AddMovementInput(FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X), Value);
	}
}

void ATartariaCharacter::MoveRight(float Value)
{
	if (Controller && Value != 0.0f)
	{
		const FRotator YawOnly(0.0f, GetControlRotation().Yaw, 0.0f);
		AddMovementInput(FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y), Value);
	}
}

void ATartariaCharacter::Turn(float Value)
{
	AddControllerYawInput(Value);
}

void ATartariaCharacter::LookUp(float Value)
{
	AddControllerPitchInput(Value);
}

void ATartariaCharacter::StartSprint()
{
	bIsSprinting = true;
	GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
}

void ATartariaCharacter::StopSprint()
{
	bIsSprinting = false;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

// -------------------------------------------------------
// Stamina System
// -------------------------------------------------------

bool ATartariaCharacter::ConsumeStamina(float Amount)
{
	if (CurrentStamina < Amount) return false;
	CurrentStamina -= Amount;
	StaminaIdleTimer = 0.f;  // reset regen delay
	OnStaminaChanged.Broadcast(CurrentStamina, MaxStamina);
	return true;
}

void ATartariaCharacter::DodgeRoll()
{
	if (bIsDodging || DodgeRollCooldownTimer > 0.0f || bIsDead) return;
	if (!ConsumeStamina(DodgeStaminaCost)) return;

	// Get movement direction (or forward if standing still)
	FVector MoveDir = GetVelocity();
	if (MoveDir.SizeSquared() < 100.0f)
	{
		MoveDir = GetActorForwardVector();
	}
	MoveDir.Z = 0.0f;
	MoveDir.Normalize();

	DodgeDirection = MoveDir;
	bIsDodging = true;
	DodgeRollTimer = 0.0f;

	UTartariaSoundManager::PlayDodge(this);

	// Brief invulnerability visual cue - lower camera
	TargetFOV = 100.0f;
}

// -------------------------------------------------------
// Interact
// -------------------------------------------------------

void ATartariaCharacter::Interact()
{
	// If dialogue is open, E-key closes it instead of interacting
	if (ATartariaGameMode* GM =
	    Cast<ATartariaGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		if (GM->DialogueWidget && GM->DialogueWidget->bIsOpen)
		{
			GM->DialogueWidget->HideDialogue();
			return;
		}
	}

	UTartariaSoundManager::PlayUIClick(this);
	DoInteractTrace();
	OnInteract();
}

void ATartariaCharacter::DoInteractTrace()
{
	UWorld* World = GetWorld();
	if (!World) return;

	FVector Start  = GetActorLocation();
	FVector End    = Start + GetActorForwardVector() * InteractReach;
	float   Radius = 60.0f;

	TArray<FHitResult> Hits;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	World->SweepMultiByChannel(
		Hits, Start, End, FQuat::Identity,
		ECollisionChannel::ECC_Visibility,
		FCollisionShape::MakeSphere(Radius), Params);

#if WITH_EDITOR
	DrawDebugSphere(World, End, Radius, 8, FColor::Yellow, false, 0.5f);
#endif

	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor) continue;
		if (HitActor->GetClass()->ImplementsInterface(UHJInteractable::StaticClass()))
		{
			APlayerController* PC = Cast<APlayerController>(GetController());
			IHJInteractable::Execute_OnInteract(HitActor, PC);
			return;
		}
	}
}

// -------------------------------------------------------
// Menu / Debug
// -------------------------------------------------------

void ATartariaCharacter::ToggleMenu()
{
	if (ATartariaGameMode* GM =
	    Cast<ATartariaGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->TogglePauseMenu();
	}
	OnMenuToggled();
}

void ATartariaCharacter::ToggleDebug()
{
	if (ATartariaGameMode* GM =
	    Cast<ATartariaGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->ToggleDebugOverlay();
	}
}

void ATartariaCharacter::ToggleDashboard()
{
	if (ATartariaGameMode* GM =
	    Cast<ATartariaGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->ToggleDashboardOverlay();
	}
}

void ATartariaCharacter::ToggleInventory()
{
	if (ATartariaGameMode* GM =
	    Cast<ATartariaGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->ToggleInventory();
	}
}

void ATartariaCharacter::OpenGovernance()
{
	if (ATartariaGameMode* GM =
	    Cast<ATartariaGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->OpenDashboardToRoute(TEXT("/game#governance"));
	}
}

// -------------------------------------------------------
// Health / Combat (Phase 2)
// -------------------------------------------------------

void ATartariaCharacter::ApplyDamage(float DamageAmount)
{
	if (bIsDead) return;

	CurrentHealth = FMath::Max(0.0f, CurrentHealth - DamageAmount);
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

	UE_LOG(LogTemp, Log, TEXT("TartariaCharacter: Took %.0f damage, HP=%.0f/%.0f"),
		DamageAmount, CurrentHealth, MaxHealth);

	// Phase 2: Combat feel — screen shake + vignette on damage
	ApplyScreenShake(FMath::Clamp(DamageAmount / MaxHealth, 0.1f, 1.0f) * 5.f);
	DamageVignetteAlpha = FMath::Clamp(DamageAmount / MaxHealth, 0.2f, 0.8f);

	if (CurrentHealth <= 0.0f)
	{
		bIsDead = true;
		OnPlayerDeath.Broadcast();
		UE_LOG(LogTemp, Warning, TEXT("TartariaCharacter: Player died!"));
	}
}

void ATartariaCharacter::Heal(float HealAmount)
{
	if (bIsDead) return;

	CurrentHealth = FMath::Min(MaxHealth, CurrentHealth + HealAmount);
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

// -------------------------------------------------------
// Phase 1: Landing Feel
// -------------------------------------------------------

void ATartariaCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	// Landing camera bump
	float FallSpeed = FMath::Abs(GetVelocity().Z);
	if (FallSpeed > 300.f)
	{
		float BumpScale = FMath::Clamp((FallSpeed - 300.f) / 700.f, 0.1f, 1.0f);
		LandingBumpOffset = -5.f * BumpScale;  // negative = camera dips
		LandingRecoveryTimer = 0.3f;
		// Light screen shake on hard landing
		if (BumpScale > 0.5f)
		{
			ApplyScreenShake(BumpScale * 2.f, 0.15f);
		}

		// Material-specific landing impact (Task #205)
		// Refresh floor material and play impact sound proportional to fall speed
		CachedFloorMaterial = GetFloorMaterial();
		FVector FootLoc = GetActorLocation() -
			FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		UTartariaSoundManager::PlayMaterialImpact(
			this, CachedFloorMaterial, FootLoc, BumpScale);
	}
}

// -------------------------------------------------------
// Phase 2: Combat Feel
// -------------------------------------------------------

void ATartariaCharacter::ApplyScreenShake(float Intensity, float Duration)
{
	ScreenShakeIntensity = FMath::Max(ScreenShakeIntensity, Intensity);
	ScreenShakeTimer = FMath::Max(ScreenShakeTimer, Duration);
}

void ATartariaCharacter::SetDamageDirection(const FVector& SourceLocation)
{
	LastDamageDirection = (SourceLocation - GetActorLocation()).GetSafeNormal();
	DamageDirectionTimer = 2.0f;  // Show indicator for 2 seconds
}

// -------------------------------------------------------
// Transit / Flight State
// -------------------------------------------------------

void ATartariaCharacter::RequestTransit(const FString& DestinationBody)
{
	if (FlightState == EFlightState::Supercruise)
	{
		UHJNotificationWidget::Toast(TEXT("Already in transit!"), EHJNotifType::Warning);
		return;
	}

	if (DestinationBody.Equals(CurrentBody, ESearchCase::IgnoreCase))
	{
		UHJNotificationWidget::Toast(TEXT("Already at this body"), EHJNotifType::Info);
		return;
	}

	ATartariaSolarSystemManager* Solar = ATartariaSolarSystemManager::Get(this);
	if (!Solar)
	{
		UHJNotificationWidget::Toast(TEXT("Solar system not available"), EHJNotifType::Error);
		return;
	}

	// Calculate transit parameters
	float DeltaV = Solar->CalculateHohmannDeltaV(CurrentBody, DestinationBody);
	float TransitTime = Solar->CalculateTransitTime(CurrentBody, DestinationBody);

	// Clamp transit time to reasonable game bounds (5s min, 120s max)
	TransitTime = FMath::Clamp(TransitTime, 5.0f, 120.0f);

	// Set up transit status
	TransitStatus.bInTransit       = true;
	TransitStatus.OriginBody       = CurrentBody;
	TransitStatus.DestinationBody  = DestinationBody;
	TransitStatus.DeltaVRequired   = DeltaV;
	TransitStatus.TransitTimeSec   = TransitTime;
	TransitStatus.ElapsedSec       = 0.f;
	TransitStatus.ProgressFraction = 0.f;

	// Enter supercruise
	FlightState = EFlightState::Supercruise;
	OnFlightStateChanged.Broadcast(FlightState);

	// Disable character movement during transit
	GetCharacterMovement()->DisableMovement();

	// Spawn warp VFX
	SpawnWarpEffect(DestinationBody);

	UHJNotificationWidget::Toast(
		FString::Printf(TEXT("Aetheric Warp: %s -> %s (dV=%.0f m/s, ETA=%.0fs)"),
			*CurrentBody, *DestinationBody, DeltaV, TransitTime),
		EHJNotifType::Info, 4.0f);

	UE_LOG(LogTemp, Log, TEXT("TartariaCharacter: Transit started %s -> %s (dV=%.0f, time=%.1fs)"),
		*CurrentBody, *DestinationBody, DeltaV, TransitTime);

	// Set timer for arrival
	GetWorldTimerManager().SetTimer(
		TransitTimerHandle,
		this,
		&ATartariaCharacter::OnTransitArrived,
		TransitTime,
		false);
}

void ATartariaCharacter::UpdateTransit(float DeltaTime)
{
	if (!TransitStatus.bInTransit) return;

	TransitStatus.ElapsedSec += DeltaTime;

	if (TransitStatus.TransitTimeSec > 0.f)
		TransitStatus.ProgressFraction =
			FMath::Clamp(TransitStatus.ElapsedSec / TransitStatus.TransitTimeSec, 0.f, 1.f);

	OnTransitProgress.Broadcast(TransitStatus.ProgressFraction, TransitStatus.DestinationBody);
}

void ATartariaCharacter::OnTransitArrived()
{
	FString Destination = TransitStatus.DestinationBody;

	// Clear transit state
	TransitStatus.bInTransit = false;
	TransitStatus.ProgressFraction = 1.0f;

	// Deactivate and destroy warp VFX
	DestroyWarpEffect();

	// Update current body
	CurrentBody = Destination;

	// Update solar system manager
	ATartariaSolarSystemManager* Solar = ATartariaSolarSystemManager::Get(this);
	if (Solar)
	{
		Solar->SetPlayerCurrentBody(Destination);
		Solar->RebaseOriginToBody(Destination);
	}

	// Trigger level stream transition (fade, hide/show, apply visual profile)
	ATartariaLevelStreamManager* LSM = ATartariaLevelStreamManager::Get(this);
	if (LSM)
	{
		LSM->TransitionToBody(Destination);
	}

	// Re-enable movement
	FlightState = EFlightState::OnFoot;
	OnFlightStateChanged.Broadcast(FlightState);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	// Teleport player to new body's surface (origin after rebase)
	SetActorLocation(FVector(0.f, 0.f, 200.f));

	UHJNotificationWidget::Toast(
		FString::Printf(TEXT("Arrived at %s"), *Destination),
		EHJNotifType::Success, 3.0f);

	UE_LOG(LogTemp, Log, TEXT("TartariaCharacter: Arrived at %s"), *Destination);
}

void ATartariaCharacter::OpenStarMap()
{
	if (ATartariaGameMode* GM =
	    Cast<ATartariaGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->OpenDashboardToRoute(TEXT("/game#starmap"));
	}
}

// -------------------------------------------------------
// Warp VFX
// -------------------------------------------------------

void ATartariaCharacter::SpawnWarpEffect(const FString& DestinationBody)
{
	DestroyWarpEffect();  // Clean up any existing

	UWorld* World = GetWorld();
	if (!World) return;

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ActiveWarpEffect = World->SpawnActor<ATartariaWarpEffect>(
		ATartariaWarpEffect::StaticClass(),
		GetActorLocation(),
		GetActorRotation(),
		Params);

	if (ActiveWarpEffect)
	{
		// Attach to player so it follows during transit
		ActiveWarpEffect->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		ActiveWarpEffect->ActivateWarp(DestinationBody);

		UE_LOG(LogTemp, Log, TEXT("TartariaCharacter: Spawned warp effect for %s"), *DestinationBody);
	}
}

void ATartariaCharacter::DestroyWarpEffect()
{
	if (ActiveWarpEffect && IsValid(ActiveWarpEffect))
	{
		ActiveWarpEffect->DeactivateWarp();
		ActiveWarpEffect->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		ActiveWarpEffect->Destroy();
		ActiveWarpEffect = nullptr;

		UE_LOG(LogTemp, Log, TEXT("TartariaCharacter: Destroyed warp effect"));
	}
}

// -------------------------------------------------------
// Weapon System
// -------------------------------------------------------

void ATartariaCharacter::EquipWeaponMesh(const FString& WeaponId)
{
	if (WeaponId.IsEmpty())
	{
		UnequipWeapon();
		return;
	}

	EquippedWeaponId = WeaponId;

	// Fetch weapon mesh from Flask backend via MeshLoader
	UHJGameInstance* GI = UHJGameInstance::Get(this);
	if (!GI || !GI->ApiClient) return;

	FString MeshEndpoint = FString::Printf(TEXT("/api/mesh/json/weapon_%s/weapon"), *WeaponId);
	TWeakObjectPtr<ATartariaCharacter> WeakThis(this);

	FOnApiResponse CB;
	CB.BindLambda([WeakThis](bool bOk, const FString& Body)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, bOk, Body]()
		{
			ATartariaCharacter* Self = WeakThis.Get();
			if (!Self || !bOk) return;

			// Use HJMeshLoader to parse JSON mesh data into ProceduralMesh
			UHJMeshLoader::LoadMeshFromJsonString(Self->WeaponMesh, Body);
			Self->WeaponMesh->SetVisibility(true);

			// Extract rarity from JSON response (default to COMMON if absent)
			FString Rarity = TEXT("COMMON");
			TSharedPtr<FJsonObject> JsonObj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
			if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
			{
				FString RarityField;
				if (JsonObj->TryGetStringField(TEXT("rarity"), RarityField))
				{
					Rarity = RarityField;
				}
			}

			// Apply rarity-based material preset
			UHJMeshLoader::ApplyRarityMaterial(Self->WeaponMesh, Rarity);

			// Fetch combat stats from mesh metadata to configure hitbox (Task #203)
			Self->FetchAndApplyWeaponCombatStats(Self->EquippedWeaponId);

			UE_LOG(LogTemp, Log, TEXT("TartariaCharacter: Weapon mesh loaded for %s (rarity=%s)"),
				*Self->EquippedWeaponId, *Rarity);
		});
	});

	GI->ApiClient->Get(MeshEndpoint, CB);
}

void ATartariaCharacter::UnequipWeapon()
{
	EquippedWeaponId = TEXT("");
	if (WeaponMesh)
	{
		WeaponMesh->ClearAllMeshSections();
		WeaponMesh->SetVisibility(false);
	}

	// Reset weapon combat stats and disable hitbox (Task #203)
	EquippedWeaponStats = FWeaponCombatStats();
	WeaponSpeedMult = 1.0f;
	DisableWeaponHitbox();
}

// -------------------------------------------------------
// Melee Combat
// -------------------------------------------------------

void ATartariaCharacter::MeleeAttack()
{
	if (bIsAttacking || bIsDodging || bIsDead) return;
	if (AttackCooldownTimer > 0.f) return;

	// Heavier weapons cost more stamina (Task #203)
	float StaminaCost = AttackStaminaCost;
	if (EquippedWeaponStats.bIsValid)
	{
		StaminaCost += EquippedWeaponStats.WeightKg * 2.0f;
	}
	if (!ConsumeStamina(StaminaCost)) return;

	bIsAttacking = true;
	bAttackHitChecked = false;
	AttackTimer = 0.f;

	UE_LOG(LogTemp, Verbose, TEXT("TartariaCharacter: Melee attack initiated"));
}

void ATartariaCharacter::OnAttackInput()
{
	MeleeAttack();
}

// -------------------------------------------------------
// Dynamic Weapon Hitboxes from CAD Data (Task #203)
// -------------------------------------------------------

void ATartariaCharacter::ApplyWeaponStats(const FWeaponCombatStats& Stats)
{
	EquippedWeaponStats = Stats;
	EquippedWeaponStats.bIsValid = true;

	// Configure the dynamic hitbox extents from mesh geometry
	if (WeaponHitbox)
	{
		WeaponHitbox->SetBoxExtent(Stats.HitboxExtents);
		UE_LOG(LogTemp, Log,
			TEXT("TartariaCharacter: WeaponHitbox extents set to (%.1f, %.1f, %.1f)"),
			Stats.HitboxExtents.X, Stats.HitboxExtents.Y, Stats.HitboxExtents.Z);
	}

	// Set weapon-weight-based attack speed modifier
	WeaponSpeedMult = FMath::Clamp(Stats.AttackSpeed, 0.3f, 1.5f);

	UE_LOG(LogTemp, Log,
		TEXT("TartariaCharacter: Applied weapon stats — reach=%.1f, weight=%.2fkg, "
		     "damage=%d, speed=%.2f, hitbox=(%.1f,%.1f,%.1f)"),
		Stats.Reach, Stats.WeightKg, Stats.BaseDamage, Stats.AttackSpeed,
		Stats.HitboxExtents.X, Stats.HitboxExtents.Y, Stats.HitboxExtents.Z);
}

void ATartariaCharacter::EnableWeaponHitbox()
{
	if (WeaponHitbox && WeaponHitbox->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		WeaponHitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
}

void ATartariaCharacter::DisableWeaponHitbox()
{
	if (WeaponHitbox && WeaponHitbox->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
	{
		WeaponHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ATartariaCharacter::FetchAndApplyWeaponCombatStats(const FString& WeaponId)
{
	UHJGameInstance* GI = UHJGameInstance::Get(this);
	if (!GI || !GI->ApiClient)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("TartariaCharacter: Cannot fetch combat stats — ApiClient unavailable"));
		return;
	}

	// Fetch combat stats from the mesh combat-stats endpoint
	FString StatsEndpoint = FString::Printf(
		TEXT("/api/mesh/combat-stats/weapon_%s/weapon"), *WeaponId);
	TWeakObjectPtr<ATartariaCharacter> WeakThis(this);

	FOnApiResponse CB;
	CB.BindLambda([WeakThis](bool bOk, const FString& Body)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, bOk, Body]()
		{
			ATartariaCharacter* Self = WeakThis.Get();
			if (!Self) return;

			if (!bOk)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("TartariaCharacter: Failed to fetch combat stats, using defaults"));
				return;
			}

			// Parse JSON response
			TSharedPtr<FJsonObject> Root;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
			if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
			{
				UE_LOG(LogTemp, Warning,
					TEXT("TartariaCharacter: Failed to parse combat stats JSON"));
				return;
			}

			bool bSuccess = false;
			Root->TryGetBoolField(TEXT("success"), bSuccess);
			if (!bSuccess) return;

			const TSharedPtr<FJsonObject>* StatsObj = nullptr;
			if (!Root->TryGetObjectField(TEXT("combat_stats"), StatsObj) || !StatsObj)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("TartariaCharacter: combat_stats object missing from response"));
				return;
			}

			// Parse combat stats into struct
			FWeaponCombatStats Stats;
			Stats.Reach = static_cast<float>((*StatsObj)->GetNumberField(TEXT("reach_cm")));
			Stats.WeightKg = static_cast<float>((*StatsObj)->GetNumberField(TEXT("weight_kg")));
			Stats.BaseDamage = static_cast<int32>((*StatsObj)->GetNumberField(TEXT("base_damage")));
			Stats.AttackSpeed = static_cast<float>((*StatsObj)->GetNumberField(TEXT("attack_speed")));

			// Parse hitbox extents array [hx, hy, hz]
			const TArray<TSharedPtr<FJsonValue>>* ExtentsArr = nullptr;
			if ((*StatsObj)->TryGetArrayField(TEXT("hitbox_extents"), ExtentsArr) &&
				ExtentsArr && ExtentsArr->Num() >= 3)
			{
				Stats.HitboxExtents = FVector(
					static_cast<float>((*ExtentsArr)[0]->AsNumber()),
					static_cast<float>((*ExtentsArr)[1]->AsNumber()),
					static_cast<float>((*ExtentsArr)[2]->AsNumber())
				);
			}

			Self->ApplyWeaponStats(Stats);
		});
	});

	GI->ApiClient->Get(StatsEndpoint, CB);
}
