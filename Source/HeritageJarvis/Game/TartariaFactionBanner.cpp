#include "TartariaFactionBanner.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"

ATartariaFactionBanner::ATartariaFactionBanner()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;  // 20 fps

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	UStaticMesh* CylMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Engine/BasicShapes/Cube.Cube"));

	// ── Pole — tall thin cylinder ────────────────────────────
	PoleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pole"));
	PoleMesh->SetupAttachment(RootComponent);
	if (CylMesh) PoleMesh->SetStaticMesh(CylMesh);
	PoleMesh->SetRelativeScale3D(FVector(0.05f, 0.05f, 1.2f));
	PoleMesh->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
	PoleMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PoleMesh->SetCollisionResponseToAllChannels(ECR_Block);

	// ── Banner cloth — flat rectangle (always present) ───────
	BannerCloth = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BannerCloth"));
	BannerCloth->SetupAttachment(RootComponent);
	if (CubeMesh) BannerCloth->SetStaticMesh(CubeMesh);
	BannerCloth->SetRelativeScale3D(FVector(0.005f, 0.35f, 0.5f));
	BannerCloth->SetRelativeLocation(FVector(5.f, 20.f, 90.f));
	BannerCloth->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BannerCloth->CastShadow = true;
}

void ATartariaFactionBanner::BeginPlay()
{
	Super::BeginPlay();

	// Apply pole material
	SetColor(PoleMesh, FLinearColor(0.3f, 0.25f, 0.18f));  // Dark wood

	RebuildLOD();
}

FLinearColor ATartariaFactionBanner::GetFactionColor() const
{
	if (FactionKey.Equals(TEXT("ARGENTUM"), ESearchCase::IgnoreCase))
		return FLinearColor(0.7f, 0.7f, 0.75f);   // Silver
	if (FactionKey.Equals(TEXT("AUREATE"), ESearchCase::IgnoreCase))
		return FLinearColor(0.85f, 0.7f, 0.2f);    // Gold
	if (FactionKey.Equals(TEXT("FERRUM"), ESearchCase::IgnoreCase))
		return FLinearColor(0.5f, 0.3f, 0.15f);    // Iron/rust
	if (FactionKey.Equals(TEXT("OBSIDIAN"), ESearchCase::IgnoreCase))
		return FLinearColor(0.15f, 0.1f, 0.2f);    // Dark purple
	return FLinearColor(0.5f, 0.5f, 0.5f);         // Default gray
}

void ATartariaFactionBanner::SetFaction(const FString& InFactionKey, float InReputation)
{
	FactionKey = InFactionKey;
	Reputation = FMath::Clamp(InReputation, 0.f, 100.f);
	RebuildLOD();
}

void ATartariaFactionBanner::UpdateReputation(float NewReputation)
{
	float OldRep = Reputation;
	Reputation = FMath::Clamp(NewReputation, 0.f, 100.f);

	// Determine if LOD threshold was crossed
	EBannerLOD NewLOD;
	if (Reputation >= 70.f)      NewLOD = EBannerLOD::Gilded;
	else if (Reputation >= 30.f) NewLOD = EBannerLOD::Embroidered;
	else                         NewLOD = EBannerLOD::Plain;

	if (NewLOD != CurrentLOD)
	{
		RebuildLOD();
	}
}

void ATartariaFactionBanner::RebuildLOD()
{
	// Determine LOD from reputation
	if (Reputation >= 70.f)      CurrentLOD = EBannerLOD::Gilded;
	else if (Reputation >= 30.f) CurrentLOD = EBannerLOD::Embroidered;
	else                         CurrentLOD = EBannerLOD::Plain;

	FLinearColor FactionColor = GetFactionColor();

	// Always: color the banner cloth
	SetColor(BannerCloth, FactionColor, CurrentLOD == EBannerLOD::Gilded ? 1.0f : 0.f);

	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	// ── Embroidered+ : Add trim border + emblem ──────────────
	if (CurrentLOD >= EBannerLOD::Embroidered)
	{
		if (!TrimBorder && CubeMesh)
		{
			TrimBorder = NewObject<UStaticMeshComponent>(this, TEXT("TrimBorder"));
			TrimBorder->SetupAttachment(RootComponent);
			TrimBorder->SetStaticMesh(CubeMesh);
			TrimBorder->SetRelativeScale3D(FVector(0.006f, 0.38f, 0.53f));
			TrimBorder->SetRelativeLocation(FVector(4.f, 20.f, 90.f));
			TrimBorder->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			TrimBorder->CastShadow = false;
			TrimBorder->RegisterComponent();
		}
		if (TrimBorder)
		{
			TrimBorder->SetVisibility(true);
			// Trim is a slightly darker/lighter version of faction color
			FLinearColor TrimColor = FactionColor * 0.6f;
			SetColor(TrimBorder, TrimColor, 0.3f);
		}

		if (!EmblemMesh && CubeMesh)
		{
			EmblemMesh = NewObject<UStaticMeshComponent>(this, TEXT("Emblem"));
			EmblemMesh->SetupAttachment(RootComponent);
			EmblemMesh->SetStaticMesh(CubeMesh);
			EmblemMesh->SetRelativeScale3D(FVector(0.008f, 0.1f, 0.1f));
			EmblemMesh->SetRelativeLocation(FVector(7.f, 20.f, 100.f));
			EmblemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			EmblemMesh->CastShadow = false;
			EmblemMesh->RegisterComponent();
		}
		if (EmblemMesh)
		{
			EmblemMesh->SetVisibility(true);
			// Emblem is contrasting color
			FLinearColor EmblemColor = FLinearColor(1.f, 1.f, 1.f) - FactionColor;
			EmblemColor.A = 1.f;
			SetColor(EmblemMesh, EmblemColor, 1.5f);
		}
	}
	else
	{
		// Plain: hide trim + emblem
		if (TrimBorder) TrimBorder->SetVisibility(false);
		if (EmblemMesh) EmblemMesh->SetVisibility(false);
	}

	// ── Gilded: Add gold frame + finial + glow ───────────────
	if (CurrentLOD == EBannerLOD::Gilded)
	{
		if (!GoldFrame && CubeMesh)
		{
			GoldFrame = NewObject<UStaticMeshComponent>(this, TEXT("GoldFrame"));
			GoldFrame->SetupAttachment(RootComponent);
			GoldFrame->SetStaticMesh(CubeMesh);
			GoldFrame->SetRelativeScale3D(FVector(0.007f, 0.40f, 0.55f));
			GoldFrame->SetRelativeLocation(FVector(3.f, 20.f, 90.f));
			GoldFrame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			GoldFrame->CastShadow = false;
			GoldFrame->RegisterComponent();
		}
		if (GoldFrame)
		{
			GoldFrame->SetVisibility(true);
			SetColor(GoldFrame, FLinearColor(0.83f, 0.66f, 0.22f), 2.0f);  // Golden emissive
		}

		if (!Finial && SphereMesh)
		{
			Finial = NewObject<UStaticMeshComponent>(this, TEXT("Finial"));
			Finial->SetupAttachment(RootComponent);
			Finial->SetStaticMesh(SphereMesh);
			Finial->SetRelativeScale3D(FVector(0.1f, 0.1f, 0.1f));
			Finial->SetRelativeLocation(FVector(0.f, 0.f, 125.f));
			Finial->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Finial->CastShadow = false;
			Finial->RegisterComponent();
		}
		if (Finial)
		{
			Finial->SetVisibility(true);
			SetColor(Finial, FLinearColor(0.83f, 0.66f, 0.22f), 3.0f);
		}

		if (!GildedGlow)
		{
			GildedGlow = NewObject<UPointLightComponent>(this, TEXT("GildedGlow"));
			GildedGlow->SetupAttachment(RootComponent);
			GildedGlow->SetRelativeLocation(FVector(10.f, 20.f, 100.f));
			GildedGlow->SetLightColor(FLinearColor(0.9f, 0.75f, 0.3f));
			GildedGlow->SetIntensity(5000.f);
			GildedGlow->SetAttenuationRadius(250.f);
			GildedGlow->CastShadows = false;
			GildedGlow->RegisterComponent();
		}
		if (GildedGlow)
			GildedGlow->SetVisibility(true);
	}
	else
	{
		if (GoldFrame) GoldFrame->SetVisibility(false);
		if (Finial) Finial->SetVisibility(false);
		if (GildedGlow) GildedGlow->SetVisibility(false);
	}

	UE_LOG(LogTemp, Log, TEXT("TartariaFactionBanner: %s rep=%.0f LOD=%s"),
		*FactionKey, Reputation,
		CurrentLOD == EBannerLOD::Gilded ? TEXT("Gilded") :
		CurrentLOD == EBannerLOD::Embroidered ? TEXT("Embroidered") : TEXT("Plain"));
}

void ATartariaFactionBanner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	AnimTime += DeltaTime;

	// Gentle banner sway (wind simulation)
	float Sway = FMath::Sin(AnimTime * 1.2f + GetActorLocation().X * 0.001f) * 3.f;
	BannerCloth->SetRelativeRotation(FRotator(Sway, 0.f, 0.f));

	// Gilded glow pulse
	if (CurrentLOD == EBannerLOD::Gilded && GildedGlow)
	{
		float Pulse = 4000.f + FMath::Sin(AnimTime * 2.5f) * 1500.f;
		GildedGlow->SetIntensity(Pulse);
	}
}

void ATartariaFactionBanner::SetColor(UStaticMeshComponent* Mesh,
	const FLinearColor& Color, float Emissive)
{
	if (!Mesh) return;
	UMaterialInstanceDynamic* Mat = Mesh->CreateAndSetMaterialInstanceDynamic(0);
	if (Mat)
	{
		Mat->SetVectorParameterValue(TEXT("Color"), Color);
		if (Emissive > 0.f)
			Mat->SetScalarParameterValue(TEXT("Emissive"), Emissive);
	}
}
