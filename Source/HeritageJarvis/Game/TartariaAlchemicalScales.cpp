#include "TartariaAlchemicalScales.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"

ATartariaAlchemicalScales::ATartariaAlchemicalScales()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.033f;  // 30 fps

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	UStaticMesh* CylMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	// ── Fulcrum pillar — tall thin cylinder ──────────────────
	FulcrumPillar = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Fulcrum"));
	FulcrumPillar->SetupAttachment(RootComponent);
	if (CylMesh) FulcrumPillar->SetStaticMesh(CylMesh);
	FulcrumPillar->SetRelativeScale3D(FVector(0.15f, 0.15f, 0.6f));
	FulcrumPillar->SetRelativeLocation(FVector(0.f, 0.f, 30.f));
	FulcrumPillar->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	FulcrumPillar->SetCollisionResponseToAllChannels(ECR_Block);

	// ── Beam — horizontal bar that tilts ─────────────────────
	BeamMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beam"));
	BeamMesh->SetupAttachment(RootComponent);
	if (CubeMesh) BeamMesh->SetStaticMesh(CubeMesh);
	BeamMesh->SetRelativeScale3D(FVector(0.03f, BEAM_HALF_LEN * 2.f / 100.f, 0.02f));
	BeamMesh->SetRelativeLocation(FVector(0.f, 0.f, BEAM_HEIGHT));
	BeamMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// ── Left pan (prosperity) — flattened sphere ─────────────
	LeftPan = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftPan"));
	LeftPan->SetupAttachment(RootComponent);
	if (SphereMesh) LeftPan->SetStaticMesh(SphereMesh);
	LeftPan->SetRelativeScale3D(FVector(0.25f, 0.25f, 0.06f));
	LeftPan->SetRelativeLocation(FVector(0.f, -BEAM_HALF_LEN, BEAM_HEIGHT - 30.f));
	LeftPan->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// ── Left chain — thin cylinder connecting beam to pan ─────
	LeftChain = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftChain"));
	LeftChain->SetupAttachment(RootComponent);
	if (CylMesh) LeftChain->SetStaticMesh(CylMesh);
	LeftChain->SetRelativeScale3D(FVector(0.02f, 0.02f, 0.15f));
	LeftChain->SetRelativeLocation(FVector(0.f, -BEAM_HALF_LEN, BEAM_HEIGHT - 15.f));
	LeftChain->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// ── Right pan (debt) — flattened sphere ───────────────────
	RightPan = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightPan"));
	RightPan->SetupAttachment(RootComponent);
	if (SphereMesh) RightPan->SetStaticMesh(SphereMesh);
	RightPan->SetRelativeScale3D(FVector(0.25f, 0.25f, 0.06f));
	RightPan->SetRelativeLocation(FVector(0.f, BEAM_HALF_LEN, BEAM_HEIGHT - 30.f));
	RightPan->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// ── Right chain ───────────────────────────────────────────
	RightChain = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightChain"));
	RightChain->SetupAttachment(RootComponent);
	if (CylMesh) RightChain->SetStaticMesh(CylMesh);
	RightChain->SetRelativeScale3D(FVector(0.02f, 0.02f, 0.15f));
	RightChain->SetRelativeLocation(FVector(0.f, BEAM_HALF_LEN, BEAM_HEIGHT - 15.f));
	RightChain->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// ── Status glow ───────────────────────────────────────────
	StatusGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("StatusGlow"));
	StatusGlow->SetupAttachment(RootComponent);
	StatusGlow->SetRelativeLocation(FVector(0.f, 0.f, BEAM_HEIGHT + 20.f));
	StatusGlow->SetLightColor(FLinearColor(0.8f, 0.7f, 0.2f));  // Default: gold
	StatusGlow->SetIntensity(4000.f);
	StatusGlow->SetAttenuationRadius(300.f);
	StatusGlow->CastShadows = false;
}

void ATartariaAlchemicalScales::BeginPlay()
{
	Super::BeginPlay();

	// Apply materials
	auto SetColor = [](UStaticMeshComponent* Mesh, const FLinearColor& Color, float Emissive = 0.f)
	{
		if (!Mesh) return;
		UMaterialInstanceDynamic* Mat = Mesh->CreateAndSetMaterialInstanceDynamic(0);
		if (Mat)
		{
			Mat->SetVectorParameterValue(TEXT("Color"), Color);
			if (Emissive > 0.f)
				Mat->SetScalarParameterValue(TEXT("Emissive"), Emissive);
		}
	};

	SetColor(FulcrumPillar, FLinearColor(0.25f, 0.2f, 0.15f));        // Bronze
	SetColor(BeamMesh, FLinearColor(0.6f, 0.5f, 0.25f), 0.5f);        // Polished brass
	SetColor(LeftPan, FLinearColor(0.8f, 0.65f, 0.2f), 1.0f);         // Gold (prosperity)
	SetColor(RightPan, FLinearColor(0.4f, 0.15f, 0.15f), 0.5f);       // Dark red (debt)
	SetColor(LeftChain, FLinearColor(0.5f, 0.45f, 0.3f));              // Aged brass
	SetColor(RightChain, FLinearColor(0.5f, 0.45f, 0.3f));

	UpdateScaleVisuals();
}

void ATartariaAlchemicalScales::UpdateTradeBalance(float NormalizedBalance)
{
	TargetBalance = FMath::Clamp(NormalizedBalance, -1.f, 1.f);
}

void ATartariaAlchemicalScales::UpdateFromCredits(int32 Credits, int32 Debt)
{
	float Total = static_cast<float>(Credits + Debt);
	if (Total <= 0.f)
	{
		TargetBalance = 0.f;
		return;
	}

	// Positive when Credits > Debt, normalized -1 to +1
	float Diff = static_cast<float>(Credits - Debt);
	TargetBalance = FMath::Clamp(Diff / FMath::Max(Total, 1.f), -1.f, 1.f);
}

void ATartariaAlchemicalScales::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	AnimTime += DeltaTime;

	// Smooth interpolation toward target
	CurrentBalance = FMath::FInterpTo(CurrentBalance, TargetBalance, DeltaTime, 2.0f);

	UpdateScaleVisuals();
}

void ATartariaAlchemicalScales::UpdateScaleVisuals()
{
	// Tilt angle: positive balance tilts left pan down (heavier with gold)
	float TiltDeg = CurrentBalance * MAX_TILT_DEG;

	// Beam tilts around X axis
	BeamMesh->SetRelativeRotation(FRotator(TiltDeg, 0.f, 0.f));

	// Calculate pan heights from tilt
	float TiltRad = FMath::DegreesToRadians(TiltDeg);
	float LeftDrop = FMath::Sin(TiltRad) * BEAM_HALF_LEN;
	float RightDrop = -LeftDrop;

	float LeftZ = BEAM_HEIGHT - 30.f + LeftDrop;
	float RightZ = BEAM_HEIGHT - 30.f + RightDrop;

	// Gentle sway oscillation
	float Sway = FMath::Sin(AnimTime * 1.5f) * 2.f;

	LeftPan->SetRelativeLocation(FVector(0.f, -BEAM_HALF_LEN, LeftZ + Sway));
	RightPan->SetRelativeLocation(FVector(0.f, BEAM_HALF_LEN, RightZ - Sway));

	// Chains follow pans
	LeftChain->SetRelativeLocation(FVector(0.f, -BEAM_HALF_LEN,
		BEAM_HEIGHT + (LeftDrop + Sway) * 0.5f - 15.f));
	RightChain->SetRelativeLocation(FVector(0.f, BEAM_HALF_LEN,
		BEAM_HEIGHT + (RightDrop - Sway) * 0.5f - 15.f));

	// Glow color: gold when prosperous, red when in debt, white when balanced
	FLinearColor GlowColor;
	if (CurrentBalance > 0.1f)
		GlowColor = FLinearColor::LerpUsingHSV(
			FLinearColor(0.9f, 0.9f, 0.7f), FLinearColor(0.9f, 0.75f, 0.2f), CurrentBalance);
	else if (CurrentBalance < -0.1f)
		GlowColor = FLinearColor::LerpUsingHSV(
			FLinearColor(0.9f, 0.9f, 0.7f), FLinearColor(0.8f, 0.2f, 0.1f), -CurrentBalance);
	else
		GlowColor = FLinearColor(0.9f, 0.9f, 0.8f);  // Balanced: warm white

	StatusGlow->SetLightColor(GlowColor);
}
