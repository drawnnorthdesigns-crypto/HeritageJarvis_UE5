#include "TartariaEnemyActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/CapsuleComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"

ATartariaEnemyActor::ATartariaEnemyActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// Reduce capsule for enemy (slightly smaller than player)
	GetCapsuleComponent()->InitCapsuleSize(35.f, 88.f);

	SetupEnemyBody();
}

void ATartariaEnemyActor::SetupEnemyBody()
{
	// All enemies use the same compound primitive humanoid with red tint

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
		TEXT("/Engine/BasicShapes/Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(
		TEXT("/Engine/BasicShapes/Cylinder"));

	UStaticMesh* CubeMesh = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;
	UStaticMesh* SphereMesh = SphereFinder.Succeeded() ? SphereFinder.Object : nullptr;
	UStaticMesh* CylMesh = CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr;

	// Torso (cube)
	EnemyTorso = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EnemyTorso"));
	EnemyTorso->SetupAttachment(RootComponent);
	if (CubeMesh) EnemyTorso->SetStaticMesh(CubeMesh);
	EnemyTorso->SetRelativeLocation(FVector(0.f, 0.f, 10.f));
	EnemyTorso->SetWorldScale3D(FVector(0.45f, 0.3f, 0.55f));
	EnemyTorso->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Head (sphere)
	EnemyHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EnemyHead"));
	EnemyHead->SetupAttachment(EnemyTorso);
	if (SphereMesh) EnemyHead->SetStaticMesh(SphereMesh);
	EnemyHead->SetRelativeLocation(FVector(0.f, 0.f, 65.f));
	EnemyHead->SetWorldScale3D(FVector(0.28f, 0.28f, 0.3f));
	EnemyHead->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Left Arm
	EnemyArmL = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EnemyArmL"));
	EnemyArmL->SetupAttachment(EnemyTorso);
	if (CylMesh) EnemyArmL->SetStaticMesh(CylMesh);
	EnemyArmL->SetRelativeLocation(FVector(0.f, -35.f, 20.f));
	EnemyArmL->SetWorldScale3D(FVector(0.1f, 0.1f, 0.4f));
	EnemyArmL->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Right Arm
	EnemyArmR = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EnemyArmR"));
	EnemyArmR->SetupAttachment(EnemyTorso);
	if (CylMesh) EnemyArmR->SetStaticMesh(CylMesh);
	EnemyArmR->SetRelativeLocation(FVector(0.f, 35.f, 20.f));
	EnemyArmR->SetWorldScale3D(FVector(0.1f, 0.1f, 0.4f));
	EnemyArmR->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Left Leg
	EnemyLegL = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EnemyLegL"));
	EnemyLegL->SetupAttachment(RootComponent);
	if (CylMesh) EnemyLegL->SetStaticMesh(CylMesh);
	EnemyLegL->SetRelativeLocation(FVector(0.f, -12.f, -45.f));
	EnemyLegL->SetWorldScale3D(FVector(0.12f, 0.12f, 0.45f));
	EnemyLegL->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Right Leg
	EnemyLegR = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EnemyLegR"));
	EnemyLegR->SetupAttachment(RootComponent);
	if (CylMesh) EnemyLegR->SetStaticMesh(CylMesh);
	EnemyLegR->SetRelativeLocation(FVector(0.f, 12.f, -45.f));
	EnemyLegR->SetWorldScale3D(FVector(0.12f, 0.12f, 0.45f));
	EnemyLegR->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Threat glow (red point light behind)
	ThreatGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("ThreatGlow"));
	ThreatGlow->SetupAttachment(EnemyTorso);
	ThreatGlow->SetRelativeLocation(FVector(-30.f, 0.f, 30.f));
	ThreatGlow->SetIntensity(3000.f);
	ThreatGlow->SetLightColor(FLinearColor(1.0f, 0.15f, 0.05f));
	ThreatGlow->SetAttenuationRadius(300.f);
}

void ATartariaEnemyActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyDifficultyAppearance();

	// Disable AI movement — enemies stand their ground
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->DisableMovement();
	}
}

void ATartariaEnemyActor::ApplyDifficultyAppearance()
{
	// Scale enemy size by difficulty (1.0x at diff 1, up to 1.5x at diff 10)
	float SizeMult = 1.0f + (FMath::Clamp(Difficulty, 1, 10) - 1) * 0.055f;
	SetActorScale3D(FVector(SizeMult));

	// Color intensity by difficulty (darker red at low, brighter/more orange at high)
	float IntensityFactor = FMath::Clamp(Difficulty / 10.0f, 0.2f, 1.0f);
	FLinearColor BaseRed(0.5f + IntensityFactor * 0.4f, 0.1f + IntensityFactor * 0.15f, 0.05f);

	ApplyColorToMesh(EnemyTorso, BaseRed);
	ApplyColorToMesh(EnemyHead, BaseRed * 1.2f);
	ApplyColorToMesh(EnemyArmL, BaseRed * 0.85f);
	ApplyColorToMesh(EnemyArmR, BaseRed * 0.85f);
	ApplyColorToMesh(EnemyLegL, BaseRed * 0.7f);
	ApplyColorToMesh(EnemyLegR, BaseRed * 0.7f);

	// Threat glow scales with difficulty
	if (ThreatGlow)
	{
		ThreatGlow->SetIntensity(2000.f + Difficulty * 500.f);
	}
}

void ATartariaEnemyActor::ApplyColorToMesh(UStaticMeshComponent* Mesh, const FLinearColor& Color)
{
	if (!Mesh) return;
	UMaterialInstanceDynamic* DynMat = Mesh->CreateAndSetMaterialInstanceDynamic(0);
	if (DynMat)
	{
		DynMat->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

void ATartariaEnemyActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ── Idle menacing animation ──
	if (!bInCombat && !bDying)
	{
		IdlePhase += DeltaTime;

		// Menacing sway (torso rocks side to side)
		if (EnemyTorso)
		{
			float Sway = FMath::Sin(IdlePhase * 1.2f) * 3.0f;
			EnemyTorso->SetRelativeRotation(FRotator(0.f, 0.f, Sway));
		}

		// Arms raise and lower aggressively
		if (EnemyArmL)
		{
			float ArmAngle = FMath::Sin(IdlePhase * 0.8f) * 10.0f - 5.0f;
			EnemyArmL->SetRelativeRotation(FRotator(ArmAngle, 0.f, -10.f));
		}
		if (EnemyArmR)
		{
			float ArmAngle = FMath::Sin(IdlePhase * 0.8f + PI) * 10.0f - 5.0f;
			EnemyArmR->SetRelativeRotation(FRotator(ArmAngle, 0.f, 10.f));
		}

		// Face player
		ACharacter* Player = UGameplayStatics::GetPlayerCharacter(this, 0);
		if (Player)
		{
			FVector ToPlayer = (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			float Yaw = FMath::Atan2(ToPlayer.Y, ToPlayer.X) * (180.f / PI);
			FRotator Current = GetActorRotation();
			FRotator Target(0.f, Yaw, 0.f);
			SetActorRotation(FMath::RInterpTo(Current, Target, DeltaTime, 5.f));
		}

		// Auto-despawn after 30 seconds if no combat initiated
		DespawnTimer += DeltaTime;
		if (DespawnTimer > 30.f)
		{
			Destroy();
			return;
		}
	}

	// ── Combat choreography ──
	if (bInCombat && !bDying)
	{
		CombatTimer += DeltaTime;

		float Phase = CombatTimer / CombatDuration;

		if (Phase < 0.3f)
		{
			// Phase 1: Both lean in (charge)
			if (EnemyTorso)
			{
				float Lean = FMath::Lerp(0.f, 15.f, Phase / 0.3f);
				EnemyTorso->SetRelativeRotation(FRotator(Lean, 0.f, 0.f));
			}
			// Arms raise for attack
			if (EnemyArmL)
				EnemyArmL->SetRelativeRotation(FRotator(-45.f * (Phase / 0.3f), 0.f, -20.f));
			if (EnemyArmR)
				EnemyArmR->SetRelativeRotation(FRotator(-45.f * (Phase / 0.3f), 0.f, 20.f));
		}
		else if (Phase < 0.5f)
		{
			// Phase 2: Strike! Arms swing down
			float StrikePhase = (Phase - 0.3f) / 0.2f;
			if (EnemyArmL)
				EnemyArmL->SetRelativeRotation(FRotator(FMath::Lerp(-45.f, 30.f, StrikePhase), 0.f, -20.f));
			if (EnemyArmR)
				EnemyArmR->SetRelativeRotation(FRotator(FMath::Lerp(-45.f, 30.f, StrikePhase), 0.f, 20.f));
		}
		else if (Phase < 0.7f)
		{
			// Phase 3: Recoil — loser gets pushed back
			float RecoilPhase = (Phase - 0.5f) / 0.2f;
			if (!bPlayerWinsCombat)
			{
				// Player loses — enemy stands tall, pushes forward
				if (EnemyTorso)
					EnemyTorso->SetRelativeRotation(FRotator(FMath::Lerp(15.f, -5.f, RecoilPhase), 0.f, 0.f));
			}
			else
			{
				// Enemy loses — stagger back
				if (EnemyTorso)
					EnemyTorso->SetRelativeRotation(FRotator(FMath::Lerp(15.f, -20.f, RecoilPhase), 0.f, FMath::Sin(RecoilPhase * PI * 3) * 10.f));
			}
		}
		else
		{
			// Phase 4: Outcome
			if (bPlayerWinsCombat && !bDefeated)
			{
				bDefeated = true;
				bDying = true;
				DeathTimer = 0.f;
				OnEnemyDefeated.Broadcast(EnemyName, RewardCredits);
			}
			else if (!bPlayerWinsCombat && Phase >= 1.0f)
			{
				// Enemy won — reset to idle
				bInCombat = false;
				CombatTimer = 0.f;
				DespawnTimer = 0.f;  // reset despawn since combat occurred
			}
		}
	}

	// ── Death dissolve ──
	if (bDying)
	{
		DeathTimer += DeltaTime;
		float DeathPhase = DeathTimer / DeathDuration;

		// Sink into ground and scale down
		FVector Loc = GetActorLocation();
		Loc.Z -= DeltaTime * 80.f;  // sink
		SetActorLocation(Loc);

		float ScaleDown = FMath::Max(0.01f, 1.0f - DeathPhase);
		SetActorScale3D(FVector(ScaleDown));

		// Fade threat glow
		if (ThreatGlow)
		{
			ThreatGlow->SetIntensity(FMath::Max(0.f, 3000.f * (1.0f - DeathPhase)));
		}

		if (DeathPhase >= 1.0f)
		{
			Destroy();
		}
	}
}

void ATartariaEnemyActor::BeginCombatChoreography(bool bPlayerWins)
{
	if (bInCombat || bDying) return;

	bInCombat = true;
	bPlayerWinsCombat = bPlayerWins;
	CombatTimer = 0.f;

	UE_LOG(LogTemp, Log, TEXT("TartariaEnemy: Combat begins with %s — Player %s"),
		*EnemyName, bPlayerWins ? TEXT("WINS") : TEXT("LOSES"));
}
