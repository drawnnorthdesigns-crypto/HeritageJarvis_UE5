#include "TartariaInstancedWealth.h"
#include "TartariaWorldSubsystem.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"

ATartariaInstancedWealth::ATartariaInstancedWealth()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f; // 20 Hz — enough for smooth lerp

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// ── HISM — all gold bars rendered in 1-3 draw calls ──
	GoldBarHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("GoldBarHISM"));
	GoldBarHISM->SetupAttachment(RootComponent);
	GoldBarHISM->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GoldBarHISM->SetCollisionResponseToAllChannels(ECR_Overlap);
	GoldBarHISM->CastShadow = true;
	GoldBarHISM->SetCullDistances(50000.f, 200000.f);
	GoldBarHISM->NumCustomDataFloats = 0;

	// Load cube mesh for gold bar shape
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh)
	{
		GoldBarHISM->SetStaticMesh(CubeMesh);
	}

	// ── Warm golden glow from the wealth pile ──
	WealthGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("WealthGlow"));
	WealthGlow->SetupAttachment(RootComponent);
	WealthGlow->SetLightColor(FLinearColor(0.9f, 0.75f, 0.3f));
	WealthGlow->SetIntensity(8000.f);
	WealthGlow->SetAttenuationRadius(500.f);
	WealthGlow->CastShadows = false;
	WealthGlow->SetRelativeLocation(FVector(0.f, 0.f, 100.f));

	// ── Transaction burst particle system (Cascade) ──
	TransactionBurstPSC = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("TransactionBurstPSC"));
	TransactionBurstPSC->SetupAttachment(RootComponent);
	TransactionBurstPSC->SetRelativeLocation(FVector(0.f, 0.f, 50.f));
	TransactionBurstPSC->bAutoActivate = false;
	// Particle asset can be assigned in editor; fires on large transactions.
	// If no asset is assigned, only the glow flash fires.
}

void ATartariaInstancedWealth::BeginPlay()
{
	Super::BeginPlay();

	// ── Apply bright gold metallic material to HISM ──
	UMaterialInstanceDynamic* GoldMat = GoldBarHISM->CreateAndSetMaterialInstanceDynamic(0);
	if (GoldMat)
	{
		// Bright gold: base color (0.9, 0.7, 0.1), metallic=1.0, roughness=0.2
		GoldMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.9f, 0.7f, 0.1f));
		GoldMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.9f, 0.7f, 0.1f));
		GoldMat->SetScalarParameterValue(TEXT("Metallic"), 1.0f);
		GoldMat->SetScalarParameterValue(TEXT("Roughness"), 0.2f);
		GoldMat->SetScalarParameterValue(TEXT("Emissive"), 0.15f);
	}

	// Store base glow intensity for flash calculations
	BaseGlowIntensity = WealthGlow->Intensity;

	// Initial build — start with 0 bars; will be populated by subsystem
	RebuildBarInstances();

	// Fetch initial credits from the world subsystem if already populated
	UWorld* World = GetWorld();
	if (World)
	{
		UTartariaWorldSubsystem* WS = World->GetSubsystem<UTartariaWorldSubsystem>();
		if (WS && WS->Credits > 0)
		{
			UpdateFromWorldState(WS->Credits);
		}
	}
}

void ATartariaInstancedWealth::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ── Smooth credit lerp ──
	// Animate CurrentDisplayedCredits toward TargetCredits
	if (CurrentDisplayedCredits != TargetCredits)
	{
		float Direction = (TargetCredits > CurrentDisplayedCredits) ? 1.f : -1.f;
		int32 Delta = FMath::CeilToInt(CreditLerpSpeed * DeltaTime);
		int32 Remaining = FMath::Abs(TargetCredits - CurrentDisplayedCredits);
		Delta = FMath::Min(Delta, Remaining);

		CurrentDisplayedCredits += static_cast<int32>(Direction) * Delta;

		// Recalculate bar count from the interpolated credit value
		int32 NewBarCount = 0;
		if (CurrentDisplayedCredits > 0)
		{
			NewBarCount = FMath::Max(1, (CurrentDisplayedCredits * BarsPerThousandCredits) / 1000);
		}
		NewBarCount = FMath::Clamp(NewBarCount, 0, MaxBars);

		if (NewBarCount != CurrentBars)
		{
			CurrentBars = NewBarCount;
			RebuildBarInstances();

			// Scale glow intensity with wealth
			float GlowScale = FMath::Clamp(static_cast<float>(CurrentBars) / 50.f, 0.5f, 5.f);
			BaseGlowIntensity = 3000.f * GlowScale;
			if (!bGlowFlashActive)
			{
				WealthGlow->SetIntensity(BaseGlowIntensity);
			}
		}
	}

	// ── Glow flash decay ──
	if (bGlowFlashActive)
	{
		GlowFlashTimer -= DeltaTime;
		if (GlowFlashTimer <= 0.f)
		{
			bGlowFlashActive = false;
			GlowFlashTimer = 0.f;
			WealthGlow->SetIntensity(BaseGlowIntensity);
		}
		else
		{
			// Smooth decay from flash back to base
			float Alpha = GlowFlashTimer / GlowFlashDuration;
			float FlashIntensity = FMath::Lerp(BaseGlowIntensity, BaseGlowIntensity * GlowFlashMultiplier, Alpha);
			WealthGlow->SetIntensity(FlashIntensity);
		}
	}

	// ── Fallback polling from world subsystem ──
	// If UpdateFromWorldState() is not being called externally (e.g., subsystem
	// was not modified), this provides a safety net by polling every 10 seconds.
	WealthUpdateTimer += DeltaTime;
	if (WealthUpdateTimer >= WealthPollIntervalSec)
	{
		WealthUpdateTimer = 0.f;

		UWorld* World = GetWorld();
		if (World)
		{
			UTartariaWorldSubsystem* WS = World->GetSubsystem<UTartariaWorldSubsystem>();
			if (WS)
			{
				UpdateFromWorldState(WS->Credits);
			}
		}
	}
}

// -------------------------------------------------------
// Live Economy Interface
// -------------------------------------------------------

void ATartariaInstancedWealth::UpdateFromWorldState(int32 CreditBalance)
{
	// Store previous target for large-transaction detection
	PreviousTargetCredits = TargetCredits;

	// Set new target — the Tick() lerp will animate toward this
	TargetCredits = FMath::Max(0, CreditBalance);

	// Detect large transactions
	int32 CreditDelta = FMath::Abs(TargetCredits - PreviousTargetCredits);
	if (CreditDelta >= LargeTransactionThreshold && PreviousTargetCredits > 0)
	{
		TriggerTransactionBurst();
	}

	UE_LOG(LogTemp, Verbose, TEXT("TartariaInstancedWealth: UpdateFromWorldState Credits=%d (Target=%d, Previous=%d, Delta=%d)"),
		CreditBalance, TargetCredits, PreviousTargetCredits, CreditDelta);
}

void ATartariaInstancedWealth::UpdateGoldCount(int32 NewCount)
{
	CurrentBars = FMath::Clamp(NewCount, 0, MaxBars);
	RebuildBarInstances();

	// Scale glow intensity with wealth
	float GlowScale = FMath::Clamp(static_cast<float>(CurrentBars) / 50.f, 0.5f, 5.f);
	BaseGlowIntensity = 3000.f * GlowScale;
	WealthGlow->SetIntensity(BaseGlowIntensity);
}

void ATartariaInstancedWealth::UpdateFromCredits(int32 Credits)
{
	// Legacy path: 1 bar per 100 credits, min 1 if any credits
	int32 Bars = Credits > 0 ? FMath::Max(1, Credits / 100) : 0;
	UpdateGoldCount(Bars);
}

// -------------------------------------------------------
// Transaction Burst VFX
// -------------------------------------------------------

void ATartariaInstancedWealth::TriggerTransactionBurst()
{
	UE_LOG(LogTemp, Log, TEXT("TartariaInstancedWealth: Large transaction detected! Triggering burst VFX."));

	// ── Fire particle burst ──
	if (TransactionBurstPSC && TransactionBurstPSC->Template)
	{
		TransactionBurstPSC->ActivateSystem(true); // bReset=true for one-shot burst
	}
	else
	{
		// No particle template assigned — spawn a simple emitter at location
		// as a fallback using the engine's default P_Burst particle if available.
		UParticleSystem* BurstFX = LoadObject<UParticleSystem>(nullptr,
			TEXT("/Engine/VFX/Particles/P_Sparks.P_Sparks"));
		if (BurstFX)
		{
			UGameplayStatics::SpawnEmitterAtLocation(
				GetWorld(),
				BurstFX,
				GetActorLocation() + FVector(0.f, 0.f, 50.f),
				FRotator::ZeroRotator,
				FVector(2.f, 2.f, 2.f),
				true,  // bAutoDestroy
				EPSCPoolMethod::None,
				true   // bAutoActivate
			);
		}
	}

	// ── Flash the glow brighter ──
	bGlowFlashActive = true;
	GlowFlashTimer = GlowFlashDuration;
	WealthGlow->SetIntensity(BaseGlowIntensity * GlowFlashMultiplier);

	// Also flash the light color to pure bright gold for the burst
	WealthGlow->SetLightColor(FLinearColor(1.0f, 0.9f, 0.4f));

	// Reset color after flash (handled approximately in Tick; set a timer for exact reset)
	FTimerHandle ColorResetHandle;
	GetWorldTimerManager().SetTimer(ColorResetHandle, [this]()
	{
		if (IsValid(this) && WealthGlow)
		{
			WealthGlow->SetLightColor(FLinearColor(0.9f, 0.75f, 0.3f));
		}
	}, GlowFlashDuration, false);
}

// -------------------------------------------------------
// Instance Rebuilding
// -------------------------------------------------------

void ATartariaInstancedWealth::RebuildBarInstances()
{
	GoldBarHISM->ClearInstances();

	if (CurrentBars <= 0) return;

	// Pre-allocate
	TArray<FTransform> Transforms;
	Transforms.Reserve(CurrentBars);

	for (int32 i = 0; i < CurrentBars; i++)
	{
		Transforms.Add(CalcBarTransform(i));
	}

	GoldBarHISM->AddInstances(Transforms, false);

	UE_LOG(LogTemp, Verbose, TEXT("TartariaInstancedWealth: Rendered %d gold bars (%d draw calls)"),
		CurrentBars, FMath::Max(1, CurrentBars / 256 + 1));
}

FTransform ATartariaInstancedWealth::CalcBarTransform(int32 Index) const
{
	/*
	 * Grid layout: 10 bars wide, stacked in rows, with layers going up.
	 *
	 * Each bar is a 20x8x8 cm gold ingot (cube scaled to proportions).
	 * Grid: 10 wide (X axis), depth grows as needed (Y axis),
	 * layers stack vertically (Z axis).
	 *
	 * Layer 0: ground level
	 * Layer 1: on top of layer 0, etc.
	 * Each layer holds BarsPerRow * RowsPerLayer bars.
	 */

	float BarW = BarSize.X + BarSpacing;  // X spacing
	float BarD = BarSize.Y + BarSpacing;  // Y spacing
	float BarH = BarSize.Z + BarSpacing;  // Z spacing

	// Determine rows per layer — use half of BarsPerRow for depth
	int32 RowsPerLayer = FMath::Max(1, BarsPerRow / 2);
	int32 BarsPerLayer = BarsPerRow * RowsPerLayer;

	// Figure out which layer, row, and column this bar is on
	int32 Layer = (BarsPerLayer > 0) ? (Index / BarsPerLayer) : 0;
	int32 IndexInLayer = (BarsPerLayer > 0) ? (Index % BarsPerLayer) : 0;
	int32 Row = (BarsPerRow > 0) ? (IndexInLayer / BarsPerRow) : 0;
	int32 Col = (BarsPerRow > 0) ? (IndexInLayer % BarsPerRow) : 0;

	// Center the grid around the actor origin
	float OffsetX = -static_cast<float>(BarsPerRow - 1) * BarW * 0.5f;
	float OffsetY = -static_cast<float>(RowsPerLayer - 1) * BarD * 0.5f;

	FVector Pos(
		OffsetX + Col * BarW,
		OffsetY + Row * BarD,
		Layer * BarH
	);

	// Scale cube (100x100x100 engine units) to ingot proportions
	FVector Scale(BarSize.X / 100.f, BarSize.Y / 100.f, BarSize.Z / 100.f);

	// Slight random rotation for organic vault feel
	// Using golden ratio * index for deterministic pseudo-random scatter
	float Yaw = FMath::Sin(static_cast<float>(Index) * 1.618f) * 3.0f;
	float Pitch = FMath::Cos(static_cast<float>(Index) * 2.718f) * 0.5f;

	return FTransform(FRotator(Pitch, Yaw, 0.f), Pos, Scale);
}
