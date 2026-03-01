#include "TartariaInstancedWealth.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"

ATartariaInstancedWealth::ATartariaInstancedWealth()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// HISM — all gold bars rendered in 1-3 draw calls
	GoldBarHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("GoldBarHISM"));
	GoldBarHISM->SetupAttachment(RootComponent);
	GoldBarHISM->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GoldBarHISM->SetCollisionResponseToAllChannels(ECR_Overlap);
	GoldBarHISM->CastShadow = true;
	GoldBarHISM->SetCullDistances(50000.f, 200000.f);
	GoldBarHISM->NumCustomDataFloats = 0;

	// Load cube mesh for gold bar
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh)
	{
		GoldBarHISM->SetStaticMesh(CubeMesh);
	}

	// Warm golden glow from the wealth pile
	WealthGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("WealthGlow"));
	WealthGlow->SetupAttachment(RootComponent);
	WealthGlow->SetLightColor(FLinearColor(0.9f, 0.75f, 0.3f));
	WealthGlow->SetIntensity(8000.f);
	WealthGlow->SetAttenuationRadius(500.f);
	WealthGlow->CastShadows = false;
	WealthGlow->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
}

void ATartariaInstancedWealth::BeginPlay()
{
	Super::BeginPlay();

	// Apply gold material to HISM
	UMaterialInstanceDynamic* GoldMat = GoldBarHISM->CreateAndSetMaterialInstanceDynamic(0);
	if (GoldMat)
	{
		GoldMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.83f, 0.66f, 0.22f));
		GoldMat->SetScalarParameterValue(TEXT("Emissive"), 0.2f);
	}

	RebuildBarInstances();
}

void ATartariaInstancedWealth::UpdateGoldCount(int32 NewCount)
{
	CurrentBars = FMath::Clamp(NewCount, 0, MaxBars);
	RebuildBarInstances();

	// Scale glow intensity with wealth
	float GlowScale = FMath::Clamp(static_cast<float>(CurrentBars) / 100.f, 0.5f, 5.f);
	WealthGlow->SetIntensity(3000.f * GlowScale);
}

void ATartariaInstancedWealth::UpdateFromCredits(int32 Credits)
{
	// 1 bar per 100 credits, min 1 if any credits
	int32 Bars = Credits > 0 ? FMath::Max(1, Credits / 100) : 0;
	UpdateGoldCount(Bars);
}

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

	UE_LOG(LogTemp, Log, TEXT("TartariaInstancedWealth: Rendered %d gold bars (%d draw calls)"),
		CurrentBars, FMath::Max(1, CurrentBars / 256 + 1));
}

FTransform ATartariaInstancedWealth::CalcBarTransform(int32 Index) const
{
	/*
	 * Stack layout: pyramid pattern
	 *
	 * Bottom layer: BarsPerRow x N rows
	 * Each subsequent layer: shifted inward by half a bar, one less per row
	 *
	 * Layer 0: bars 0..BarsPerRow*RowsInLayer0
	 * Layer 1: offset up by BarSize.Z + BarSpacing, shifted 0.5 bar inward
	 * etc.
	 */

	float BarW = BarSize.X + BarSpacing;
	float BarD = BarSize.Y + BarSpacing;
	float BarH = BarSize.Z + BarSpacing;

	// Figure out which layer this bar is on
	int32 BarsPlaced = 0;
	int32 Layer = 0;
	int32 LayerBPR = BarsPerRow;
	int32 LayerRows = FMath::Max(1, BarsPerRow / 2);

	while (BarsPlaced + LayerBPR * LayerRows <= Index && LayerBPR > 1)
	{
		BarsPlaced += LayerBPR * LayerRows;
		Layer++;
		LayerBPR = FMath::Max(1, LayerBPR - 1);
		LayerRows = FMath::Max(1, LayerRows - 1);
	}

	int32 IndexInLayer = Index - BarsPlaced;
	int32 Row = (LayerBPR > 0) ? (IndexInLayer / LayerBPR) : 0;
	int32 Col = (LayerBPR > 0) ? (IndexInLayer % LayerBPR) : 0;

	// Center the layer
	float LayerOffsetX = -static_cast<float>(LayerBPR - 1) * BarW * 0.5f;
	float LayerOffsetY = -static_cast<float>(LayerRows - 1) * BarD * 0.5f;

	FVector Pos(
		LayerOffsetX + Col * BarW,
		LayerOffsetY + Row * BarD,
		Layer * BarH
	);

	// Scale cube to bar proportions: Cube is 100x100x100, we want BarSize
	FVector Scale(BarSize.X / 100.f, BarSize.Y / 100.f, BarSize.Z / 100.f);

	// Slight random rotation for visual variety
	float Yaw = FMath::Sin(static_cast<float>(Index) * 1.618f) * 3.0f; // Golden ratio scatter

	return FTransform(FRotator(0.f, Yaw, 0.f), Pos, Scale);
}
