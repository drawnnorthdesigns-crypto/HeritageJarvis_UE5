#include "TartariaResonanceSpire.h"
#include "Core/HJGameInstance.h"
#include "Core/HJApiClient.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Async/Async.h"
#include "Kismet/GameplayStatics.h"

// -------------------------------------------------------
// All dimensions in centimeters (UE5 convention).
// 500m spire = 50,000 cm.
// -------------------------------------------------------

ATartariaResonanceSpire::ATartariaResonanceSpire()
{
	PrimaryActorTick.bCanEverTick = true;

	// ── Root component ──────────────────────────────────────
	SpireRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SpireRoot"));
	RootComponent = SpireRoot;

	BuildSpireStructure();
}

// -------------------------------------------------------
// Helper: create one static-mesh part, attach, color it
// -------------------------------------------------------
UStaticMeshComponent* ATartariaResonanceSpire::CreateSpirePart(
	const FName& Name, const TCHAR* ShapePath,
	USceneComponent* Parent, const FVector& RelLoc, const FVector& Scale,
	const FLinearColor& Color)
{
	UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	if (!Part) return nullptr;

	Part->SetupAttachment(Parent);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(
		TEXT("/Engine/BasicShapes/Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
		TEXT("/Engine/BasicShapes/Sphere"));

	// Match the requested shape path to the correct finder
	if (FCString::Strcmp(ShapePath, TEXT("/Engine/BasicShapes/Cylinder")) == 0)
	{
		if (CylinderFinder.Succeeded())
			Part->SetStaticMesh(CylinderFinder.Object);
	}
	else if (FCString::Strcmp(ShapePath, TEXT("/Engine/BasicShapes/Sphere")) == 0)
	{
		if (SphereFinder.Succeeded())
			Part->SetStaticMesh(SphereFinder.Object);
	}

	Part->SetRelativeLocation(RelLoc);
	Part->SetRelativeScale3D(Scale);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	return Part;
}

// -------------------------------------------------------
// Build the compound primitive spire structure
// -------------------------------------------------------
void ATartariaResonanceSpire::BuildSpireStructure()
{
	// All measurements in cm.  Engine cylinder is 100cm tall (unit scale),
	// so a cylinder with Z-scale = S gives height = 100 * S cm.
	// Engine sphere is 100cm diameter at unit scale (radius 50cm).

	const TCHAR* Cyl = TEXT("/Engine/BasicShapes/Cylinder");
	const TCHAR* Sph = TEXT("/Engine/BasicShapes/Sphere");

	// ── Base Platform ─────────────────────────────────────
	// 80m radius = 8000cm.  Engine cylinder at scale 1 has radius 50cm.
	// Scale XY = 8000/50 = 160.  Height 10m = 1000cm; cylinder is 100cm at scale 1 → Z = 10.
	BasePlatform = CreateSpirePart(
		FName("BasePlatform"), Cyl,
		SpireRoot,
		FVector(0.f, 0.f, 0.f),                    // sits at world origin of actor
		FVector(160.f, 160.f, 10.f),                // 80m radius, 10m tall
		FLinearColor(0.15f, 0.12f, 0.1f));          // dark stone

	// ── Lower Shaft ───────────────────────────────────────
	// 20m radius → XY = 20*100/50 = 40.  300m tall → Z = 300*100/100 = 300.
	// Positioned so bottom rests on base top: base top at ~500cm, shaft center at 500 + 15000 = 15500cm.
	LowerShaft = CreateSpirePart(
		FName("LowerShaft"), Cyl,
		SpireRoot,
		FVector(0.f, 0.f, 15500.f),                // center of 300m shaft
		FVector(40.f, 40.f, 300.f),                 // 20m radius, 300m tall
		FLinearColor(0.2f, 0.18f, 0.15f));          // dark metal

	// ── Upper Shaft ───────────────────────────────────────
	// 12m radius → XY = 24.  200m tall → Z = 200.
	// Starts where lower shaft ends (base + 300m = 30500cm), center at 30500 + 10000 = 40500.
	UpperShaft = CreateSpirePart(
		FName("UpperShaft"), Cyl,
		SpireRoot,
		FVector(0.f, 0.f, 40500.f),                // center of 200m shaft
		FVector(24.f, 24.f, 200.f),                 // 12m radius, 200m tall
		FLinearColor(0.3f, 0.25f, 0.2f));           // lighter metal

	// ── Mid Ring ──────────────────────────────────────────
	// 30m radius → XY = 60.  5m tall → Z = 5.  At 250m height = 25000cm.
	MidRing = CreateSpirePart(
		FName("MidRing"), Cyl,
		SpireRoot,
		FVector(0.f, 0.f, 25000.f),
		FVector(60.f, 60.f, 5.f),                   // 30m radius, 5m tall
		FLinearColor(0.78f, 0.63f, 0.3f));          // gold accent

	// ── Crown Crystal ─────────────────────────────────────
	// 25m radius sphere → scale = 25*100/50 = 50 per axis.  At 500m = 50000cm.
	CrownCrystal = CreateSpirePart(
		FName("CrownCrystal"), Sph,
		SpireRoot,
		FVector(0.f, 0.f, 50000.f),
		FVector(50.f, 50.f, 50.f),                  // 25m radius sphere
		FLinearColor(0.9f, 0.7f, 0.3f));            // luminous gold

	// ── Crown Spikes ──────────────────────────────────────
	// 4 cylinders, 3m radius → XY = 6, 40m tall → Z = 40.
	// Placed around crown, rotated outward at 15 degrees.
	// Offset from crown center by ~30m (3000cm) in each cardinal direction.
	const float SpikeOffset = 3000.f;
	const FVector SpikeScale(6.f, 6.f, 40.f);

	CrownSpike1 = CreateSpirePart(
		FName("CrownSpike1"), Cyl,
		SpireRoot,
		FVector(SpikeOffset, 0.f, 50000.f),
		SpikeScale,
		FLinearColor(0.4f, 0.6f, 0.9f));           // crystal blue

	CrownSpike2 = CreateSpirePart(
		FName("CrownSpike2"), Cyl,
		SpireRoot,
		FVector(-SpikeOffset, 0.f, 50000.f),
		SpikeScale,
		FLinearColor(0.4f, 0.6f, 0.9f));

	CrownSpike3 = CreateSpirePart(
		FName("CrownSpike3"), Cyl,
		SpireRoot,
		FVector(0.f, SpikeOffset, 50000.f),
		SpikeScale,
		FLinearColor(0.4f, 0.6f, 0.9f));

	CrownSpike4 = CreateSpirePart(
		FName("CrownSpike4"), Cyl,
		SpireRoot,
		FVector(0.f, -SpikeOffset, 50000.f),
		SpikeScale,
		FLinearColor(0.4f, 0.6f, 0.9f));

	// Tilt spikes outward 15 degrees
	if (CrownSpike1) CrownSpike1->SetRelativeRotation(FRotator(0.f, 0.f, -15.f));
	if (CrownSpike2) CrownSpike2->SetRelativeRotation(FRotator(0.f, 0.f, 15.f));
	if (CrownSpike3) CrownSpike3->SetRelativeRotation(FRotator(-15.f, 0.f, 0.f));
	if (CrownSpike4) CrownSpike4->SetRelativeRotation(FRotator(15.f, 0.f, 0.f));

	// ── Lighting ──────────────────────────────────────────

	// Crown Beacon — enormous intensity, visible from 5km (500,000cm)
	CrownBeacon = CreateDefaultSubobject<UPointLightComponent>(TEXT("CrownBeacon"));
	CrownBeacon->SetupAttachment(SpireRoot);
	CrownBeacon->SetRelativeLocation(FVector(0.f, 0.f, 50000.f));
	CrownBeacon->SetIntensity(300000.f);
	CrownBeacon->SetLightColor(FLinearColor(1.0f, 0.85f, 0.4f));  // golden
	CrownBeacon->SetAttenuationRadius(500000.f);                   // 5km

	// Base Glow — warm amber at the foundation
	BaseGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("BaseGlow"));
	BaseGlow->SetupAttachment(SpireRoot);
	BaseGlow->SetRelativeLocation(FVector(0.f, 0.f, 1000.f));
	BaseGlow->SetIntensity(50000.f);
	BaseGlow->SetLightColor(FLinearColor(1.0f, 0.7f, 0.3f));      // warm amber
	BaseGlow->SetAttenuationRadius(100000.f);                      // 1km

	// Mid Glow — gold light at the ring
	MidGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("MidGlow"));
	MidGlow->SetupAttachment(SpireRoot);
	MidGlow->SetRelativeLocation(FVector(0.f, 0.f, 25000.f));
	MidGlow->SetIntensity(80000.f);
	MidGlow->SetLightColor(FLinearColor(1.0f, 0.8f, 0.35f));      // gold
	MidGlow->SetAttenuationRadius(200000.f);                       // 2km
}

// -------------------------------------------------------
// BeginPlay — apply dynamic material colors
// -------------------------------------------------------
void ATartariaResonanceSpire::BeginPlay()
{
	Super::BeginPlay();

	// Apply initial dynamic material colors so we can update them at runtime.
	// The constructor already set the mesh but dynamic material instances
	// must be created after the actor is registered in the world.

	auto ApplyColor = [](UStaticMeshComponent* Mesh, const FLinearColor& Color)
	{
		if (!Mesh) return;
		UMaterialInstanceDynamic* DynMat = Mesh->CreateAndSetMaterialInstanceDynamic(0);
		if (DynMat)
		{
			DynMat->SetVectorParameterValue(TEXT("Color"), Color);
		}
	};

	ApplyColor(BasePlatform, FLinearColor(0.15f, 0.12f, 0.1f));
	ApplyColor(LowerShaft,   FLinearColor(0.2f, 0.18f, 0.15f));
	ApplyColor(UpperShaft,   FLinearColor(0.3f, 0.25f, 0.2f));
	ApplyColor(MidRing,      FLinearColor(0.78f, 0.63f, 0.3f));
	ApplyColor(CrownCrystal, FLinearColor(0.9f, 0.7f, 0.3f));
	ApplyColor(CrownSpike1,  FLinearColor(0.4f, 0.6f, 0.9f));
	ApplyColor(CrownSpike2,  FLinearColor(0.4f, 0.6f, 0.9f));
	ApplyColor(CrownSpike3,  FLinearColor(0.4f, 0.6f, 0.9f));
	ApplyColor(CrownSpike4,  FLinearColor(0.4f, 0.6f, 0.9f));

	// Compute initial target intensity from starting HermeticPhase
	TargetBeaconIntensity = FMath::Lerp(MinBeaconIntensity, MaxBeaconIntensity, HermeticPhase);
	CurrentBeaconIntensity = TargetBeaconIntensity;

	UE_LOG(LogTemp, Log, TEXT("TartariaResonanceSpire: Beacon active at Z=50000cm, "
		"initial hermetic phase %.2f"), HermeticPhase);
}

// -------------------------------------------------------
// Tick — animations, beacon pulsing, backend polling
// -------------------------------------------------------
void ATartariaResonanceSpire::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateBeaconFromPhase(DeltaTime);

	// ── Crown crystal slow rotation (5 deg/sec) ─────────
	CrownRotation += DeltaTime * 5.f;
	if (CrownRotation > 360.f) CrownRotation -= 360.f;

	if (CrownCrystal)
	{
		CrownCrystal->SetRelativeRotation(FRotator(0.f, CrownRotation, 0.f));
	}

	// ── Crown spikes counter-rotation (3 deg/sec opposite) ──
	float SpikeYaw = -CrownRotation * 0.6f; // 3/5 ratio, opposite direction
	if (CrownSpike1) CrownSpike1->SetRelativeRotation(FRotator(0.f, SpikeYaw, -15.f));
	if (CrownSpike2) CrownSpike2->SetRelativeRotation(FRotator(0.f, SpikeYaw, 15.f));
	if (CrownSpike3) CrownSpike3->SetRelativeRotation(FRotator(-15.f, SpikeYaw, 0.f));
	if (CrownSpike4) CrownSpike4->SetRelativeRotation(FRotator(15.f, SpikeYaw, 0.f));

	// ── Backend polling ─────────────────────────────────
	PollTimer += DeltaTime;
	if (PollTimer >= PollInterval)
	{
		PollTimer = 0.f;
		PollHermeticPhase();
	}
}

// -------------------------------------------------------
// Beacon intensity + color modulation
// -------------------------------------------------------
void ATartariaResonanceSpire::UpdateBeaconFromPhase(float DeltaTime)
{
	// ── Sinusoidal pulse at 0.3 Hz ──────────────────────
	PulsePhase += DeltaTime * 0.3f * 2.f * PI;
	if (PulsePhase > 2.f * PI) PulsePhase -= 2.f * PI;

	// Pulse amplitude: +/- 15% of current intensity
	float PulseMultiplier = 1.0f + FMath::Sin(PulsePhase) * 0.15f;

	// ── Smoothly interpolate toward target intensity ────
	CurrentBeaconIntensity = FMath::FInterpTo(
		CurrentBeaconIntensity, TargetBeaconIntensity, DeltaTime, 1.5f);

	float FinalIntensity = CurrentBeaconIntensity * PulseMultiplier;

	if (CrownBeacon)
	{
		CrownBeacon->SetIntensity(FinalIntensity);
	}

	// ── Crown color shift based on hermetic phase ───────
	// Low phase (< 0.3): cool blue.  High phase (> 0.7): blazing gold-white.
	FLinearColor CrownColor;
	if (HermeticPhase < 0.3f)
	{
		// Lerp from cool blue to neutral gold
		float T = HermeticPhase / 0.3f;
		CrownColor = FMath::Lerp(
			FLinearColor(0.3f, 0.5f, 0.9f),         // cool blue
			FLinearColor(0.9f, 0.7f, 0.3f),          // neutral gold
			T);
	}
	else if (HermeticPhase > 0.7f)
	{
		// Lerp from neutral gold to blazing gold-white
		float T = (HermeticPhase - 0.7f) / 0.3f;
		CrownColor = FMath::Lerp(
			FLinearColor(0.9f, 0.7f, 0.3f),          // neutral gold
			FLinearColor(1.0f, 0.95f, 0.8f),          // blazing gold-white
			T);
	}
	else
	{
		// Mid-range: steady gold
		CrownColor = FLinearColor(0.9f, 0.7f, 0.3f);
	}

	// Apply color to crown crystal dynamic material
	if (CrownCrystal)
	{
		UMaterialInstanceDynamic* DynMat = Cast<UMaterialInstanceDynamic>(
			CrownCrystal->GetMaterial(0));
		if (DynMat)
		{
			DynMat->SetVectorParameterValue(TEXT("Color"), CrownColor);
		}
	}

	// Also shift the crown beacon light color
	if (CrownBeacon)
	{
		CrownBeacon->SetLightColor(CrownColor);
	}

	// ── Mid glow pulses gently in sympathy ──────────────
	if (MidGlow)
	{
		float MidPulse = 80000.f * (1.0f + FMath::Sin(PulsePhase * 0.7f) * 0.1f);
		MidGlow->SetIntensity(MidPulse * FMath::Lerp(0.5f, 1.0f, HermeticPhase));
	}

	// ── Base glow: steady warm ──────────────────────────
	if (BaseGlow)
	{
		float BaseInt = 50000.f * FMath::Lerp(0.6f, 1.0f, HermeticPhase);
		BaseGlow->SetIntensity(BaseInt);
	}
}

// -------------------------------------------------------
// Poll Python backend for hermetic cycle phase
// -------------------------------------------------------
void ATartariaResonanceSpire::PollHermeticPhase()
{
	UHJGameInstance* GI = UHJGameInstance::Get(this);
	if (!GI || !GI->ApiClient) return;

	TWeakObjectPtr<ATartariaResonanceSpire> WeakThis(this);

	FOnApiResponse CB;
	CB.BindLambda([WeakThis](bool bOk, const FString& RespBody)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, bOk, RespBody]()
		{
			ATartariaResonanceSpire* Self = WeakThis.Get();
			if (!Self) return;

			if (!bOk)
			{
				// Backend unreachable — keep last known phase
				return;
			}

			// Parse JSON: { ..., "hermetic_cycle": { "phase": 0.72, ... }, ... }
			TSharedPtr<FJsonObject> Json;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RespBody);
			if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
				return;

			const TSharedPtr<FJsonObject>* HermeticObj = nullptr;
			if (Json->TryGetObjectField(TEXT("hermetic_cycle"), HermeticObj) && HermeticObj && (*HermeticObj).IsValid())
			{
				double Phase = 0.5;
				if ((*HermeticObj)->TryGetNumberField(TEXT("phase"), Phase))
				{
					Self->HermeticPhase = FMath::Clamp((float)Phase, 0.f, 1.f);
					Self->TargetBeaconIntensity = FMath::Lerp(
						Self->MinBeaconIntensity,
						Self->MaxBeaconIntensity,
						Self->HermeticPhase);

					UE_LOG(LogTemp, Verbose,
						TEXT("ResonanceSpire: Hermetic phase updated to %.3f → target intensity %.0f"),
						Self->HermeticPhase, Self->TargetBeaconIntensity);
				}
			}
		});
	});

	GI->ApiClient->Get(TEXT("/api/game/status/full"), CB);
}
