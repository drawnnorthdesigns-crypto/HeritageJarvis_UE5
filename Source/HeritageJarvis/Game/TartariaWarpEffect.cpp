#include "TartariaWarpEffect.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"

// Static constexpr definitions
constexpr float ATartariaWarpEffect::RING_SPEEDS[4];
constexpr float ATartariaWarpEffect::RING_RADII[4];

ATartariaWarpEffect::ATartariaWarpEffect()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	WarpRoot = CreateDefaultSubobject<USceneComponent>(TEXT("WarpRoot"));
	RootComponent = WarpRoot;

	// Central warp glow — bright blue-white
	WarpGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("WarpGlow"));
	WarpGlow->SetupAttachment(WarpRoot);
	WarpGlow->SetRelativeLocation(FVector(200.f, 0.f, 0.f));
	WarpGlow->SetLightColor(FLinearColor(0.4f, 0.6f, 1.0f));
	WarpGlow->SetIntensity(0.f);  // Off until activated
	WarpGlow->SetAttenuationRadius(800.f);
	WarpGlow->CastShadows = false;

	// Exit point glow — ahead of player
	ExitGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("ExitGlow"));
	ExitGlow->SetupAttachment(WarpRoot);
	ExitGlow->SetRelativeLocation(FVector(1000.f, 0.f, 0.f));
	ExitGlow->SetLightColor(FLinearColor(0.8f, 0.7f, 1.0f));
	ExitGlow->SetIntensity(0.f);
	ExitGlow->SetAttenuationRadius(1200.f);
	ExitGlow->CastShadows = false;

	BuildWarpGeometry();
}

void ATartariaWarpEffect::BuildWarpGeometry()
{
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!CubeMesh) return;

	const FLinearColor RingColors[4] = {
		FLinearColor(0.3f, 0.5f, 1.0f),   // Inner: bright blue
		FLinearColor(0.5f, 0.3f, 0.9f),   // Mid-inner: purple
		FLinearColor(0.2f, 0.7f, 0.8f),   // Mid-outer: teal
		FLinearColor(0.6f, 0.4f, 1.0f),   // Outer: violet
	};

	// Build 4 rings, each made of SEGMENTS_PER_RING cube segments
	for (int32 Ring = 0; Ring < 4; Ring++)
	{
		float Radius = RING_RADII[Ring];

		for (int32 Seg = 0; Seg < SEGMENTS_PER_RING; Seg++)
		{
			float Angle = (static_cast<float>(Seg) / SEGMENTS_PER_RING) * 360.f;
			float AngleRad = FMath::DegreesToRadians(Angle);

			FVector SegLoc(
				0.f,
				FMath::Cos(AngleRad) * Radius,
				FMath::Sin(AngleRad) * Radius
			);

			FName SegName = *FString::Printf(TEXT("Ring%d_Seg%d"), Ring, Seg);
			UStaticMeshComponent* SegComp = CreateDefaultSubobject<UStaticMeshComponent>(SegName);
			SegComp->SetupAttachment(WarpRoot);
			SegComp->SetStaticMesh(CubeMesh);
			SegComp->SetRelativeLocation(SegLoc);
			SegComp->SetRelativeRotation(FRotator(0.f, 0.f, Angle));
			SegComp->SetRelativeScale3D(FVector(0.03f, 0.15f, 0.02f));  // Thin arc segment
			SegComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			SegComp->CastShadow = false;
			SegComp->SetVisibility(false);  // Hidden until activated

			RingSegments.Add(SegComp);
		}
	}

	// Build 8 streak bars (speed lines along forward axis)
	for (int32 i = 0; i < 8; i++)
	{
		float Angle = (static_cast<float>(i) / 8.f) * 360.f;
		float AngleRad = FMath::DegreesToRadians(Angle);
		float StreakRadius = 150.f + (i % 3) * 80.f;

		FVector StreakLoc(
			300.f + (i % 4) * 100.f,  // Staggered along forward axis
			FMath::Cos(AngleRad) * StreakRadius,
			FMath::Sin(AngleRad) * StreakRadius
		);

		FName StreakName = *FString::Printf(TEXT("Streak_%d"), i);
		UStaticMeshComponent* Streak = CreateDefaultSubobject<UStaticMeshComponent>(StreakName);
		Streak->SetupAttachment(WarpRoot);
		Streak->SetStaticMesh(CubeMesh);
		Streak->SetRelativeLocation(StreakLoc);
		Streak->SetRelativeRotation(FRotator(0.f, 0.f, Angle));
		Streak->SetRelativeScale3D(FVector(0.5f, 0.01f, 0.01f));  // Long thin streaks
		Streak->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Streak->CastShadow = false;
		Streak->SetVisibility(false);

		StreakBars.Add(Streak);
	}
}

void ATartariaWarpEffect::ActivateWarp(const FString& DestinationBody)
{
	bWarpActive = true;
	Destination = DestinationBody;
	WarpTime = 0.f;
	SetActorTickEnabled(true);

	// Choose warp color based on destination
	if (DestinationBody.Equals(TEXT("mars"), ESearchCase::IgnoreCase))
		WarpColor = FLinearColor(0.8f, 0.3f, 0.1f);  // Red-orange
	else if (DestinationBody.Equals(TEXT("moon"), ESearchCase::IgnoreCase))
		WarpColor = FLinearColor(0.6f, 0.6f, 0.7f);  // Silver
	else if (DestinationBody.Equals(TEXT("europa"), ESearchCase::IgnoreCase))
		WarpColor = FLinearColor(0.2f, 0.5f, 0.9f);  // Ice blue
	else if (DestinationBody.Equals(TEXT("titan"), ESearchCase::IgnoreCase))
		WarpColor = FLinearColor(0.7f, 0.5f, 0.15f);  // Orange-amber
	else if (DestinationBody.Equals(TEXT("venus"), ESearchCase::IgnoreCase))
		WarpColor = FLinearColor(0.8f, 0.6f, 0.2f);  // Sulfuric amber
	else if (DestinationBody.Equals(TEXT("mercury"), ESearchCase::IgnoreCase))
		WarpColor = FLinearColor(0.9f, 0.85f, 0.7f);  // Bright white-gold
	else if (DestinationBody.Equals(TEXT("deep_space"), ESearchCase::IgnoreCase))
		WarpColor = FLinearColor(0.1f, 0.1f, 0.3f);  // Deep void
	else if (DestinationBody.Equals(TEXT("tartaria_prime"), ESearchCase::IgnoreCase))
		WarpColor = FLinearColor(0.6f, 0.3f, 0.8f);  // Royal purple-gold
	else if (DestinationBody.Equals(TEXT("earth"), ESearchCase::IgnoreCase))
		WarpColor = FLinearColor(0.3f, 0.5f, 1.0f);  // Earth blue
	else
		WarpColor = FLinearColor(0.4f, 0.6f, 1.0f);  // Default blue-white

	// Apply materials to ring segments
	for (int32 i = 0; i < RingSegments.Num(); i++)
	{
		UStaticMeshComponent* Seg = RingSegments[i];
		Seg->SetVisibility(true);

		UMaterialInstanceDynamic* Mat = Seg->CreateAndSetMaterialInstanceDynamic(0);
		if (Mat)
		{
			int32 RingIndex = i / SEGMENTS_PER_RING;
			float Brightness = 1.0f - (static_cast<float>(RingIndex) * 0.15f);
			FLinearColor SegColor = WarpColor * Brightness;
			Mat->SetVectorParameterValue(TEXT("Color"), SegColor);
			Mat->SetScalarParameterValue(TEXT("Emissive"), 3.0f);
		}
	}

	// Apply materials to streaks
	for (int32 i = 0; i < StreakBars.Num(); i++)
	{
		UStaticMeshComponent* Streak = StreakBars[i];
		Streak->SetVisibility(true);

		UMaterialInstanceDynamic* Mat = Streak->CreateAndSetMaterialInstanceDynamic(0);
		if (Mat)
		{
			Mat->SetVectorParameterValue(TEXT("Color"), WarpColor * 1.5f);
			Mat->SetScalarParameterValue(TEXT("Emissive"), 5.0f);
		}
	}

	// Turn on glow lights
	WarpGlow->SetLightColor(WarpColor);
	WarpGlow->SetIntensity(15000.f);
	ExitGlow->SetLightColor(WarpColor * 0.8f);
	ExitGlow->SetIntensity(8000.f);

	// Target wider FOV for warp effect
	TargetFOV = 120.f;

	UE_LOG(LogTemp, Log, TEXT("TartariaWarpEffect: Activated (destination: %s)"), *DestinationBody);
}

void ATartariaWarpEffect::DeactivateWarp()
{
	bWarpActive = false;
	TargetFOV = 90.f;  // Return to normal

	// Hide all geometry
	for (UStaticMeshComponent* Seg : RingSegments)
	{
		if (Seg) Seg->SetVisibility(false);
	}
	for (UStaticMeshComponent* Streak : StreakBars)
	{
		if (Streak) Streak->SetVisibility(false);
	}

	// Turn off lights
	WarpGlow->SetIntensity(0.f);
	ExitGlow->SetIntensity(0.f);

	UE_LOG(LogTemp, Log, TEXT("TartariaWarpEffect: Deactivated"));

	// Keep ticking briefly to restore FOV, then stop
	// (UpdateCameraFOV will handle the smooth return)
}

void ATartariaWarpEffect::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bWarpActive)
	{
		AnimateWarp(DeltaTime);
	}

	UpdateCameraFOV(DeltaTime);

	// If deactivated and FOV is restored, stop ticking
	if (!bWarpActive && FMath::Abs(CurrentFOVAdjust) < 0.5f)
	{
		CurrentFOVAdjust = 0.f;
		SetActorTickEnabled(false);
	}
}

void ATartariaWarpEffect::AnimateWarp(float DeltaTime)
{
	WarpTime += DeltaTime;

	// Rotate ring segments
	for (int32 i = 0; i < RingSegments.Num(); i++)
	{
		UStaticMeshComponent* Seg = RingSegments[i];
		if (!Seg) continue;

		int32 RingIndex = i / SEGMENTS_PER_RING;
		int32 SegIndex = i % SEGMENTS_PER_RING;

		float Speed = RING_SPEEDS[RingIndex];
		float Radius = RING_RADII[RingIndex];

		// Base angle + rotation over time
		float BaseAngle = (static_cast<float>(SegIndex) / SEGMENTS_PER_RING) * 360.f;
		float CurrentAngle = BaseAngle + WarpTime * Speed;
		float AngleRad = FMath::DegreesToRadians(CurrentAngle);

		// Oscillate radius slightly for breathing effect
		float RadiusOsc = Radius + FMath::Sin(WarpTime * 2.f + RingIndex * 1.5f) * 15.f;

		FVector NewLoc(
			FMath::Sin(WarpTime * 0.5f + SegIndex * 0.3f) * 20.f,  // Forward sway
			FMath::Cos(AngleRad) * RadiusOsc,
			FMath::Sin(AngleRad) * RadiusOsc
		);

		Seg->SetRelativeLocation(NewLoc);
		Seg->SetRelativeRotation(FRotator(0.f, WarpTime * 30.f, CurrentAngle));
	}

	// Animate streak bars — they rush past the player
	for (int32 i = 0; i < StreakBars.Num(); i++)
	{
		UStaticMeshComponent* Streak = StreakBars[i];
		if (!Streak) continue;

		FVector Loc = Streak->GetRelativeLocation();

		// Move streaks backward (toward player, simulating forward motion)
		Loc.X -= DeltaTime * 800.f;

		// Wrap around when behind player
		if (Loc.X < -200.f)
		{
			Loc.X = 800.f + FMath::FRandRange(0.f, 200.f);

			// Randomize lateral position slightly on reset
			float Angle = FMath::FRandRange(0.f, 360.f);
			float Radius = 150.f + FMath::FRandRange(0.f, 200.f);
			Loc.Y = FMath::Cos(FMath::DegreesToRadians(Angle)) * Radius;
			Loc.Z = FMath::Sin(FMath::DegreesToRadians(Angle)) * Radius;

			// Randomize streak length
			float Len = FMath::FRandRange(0.3f, 0.8f);
			Streak->SetRelativeScale3D(FVector(Len, 0.01f, 0.01f));
		}

		Streak->SetRelativeLocation(Loc);
	}

	// Pulse glow intensity
	float GlowPulse = 12000.f + FMath::Sin(WarpTime * 4.f) * 5000.f;
	WarpGlow->SetIntensity(GlowPulse);

	float ExitPulse = 6000.f + FMath::Sin(WarpTime * 3.f + 1.f) * 3000.f;
	ExitGlow->SetIntensity(ExitPulse);
}

void ATartariaWarpEffect::UpdateCameraFOV(float DeltaTime)
{
	float DesiredAdjust = TargetFOV - 90.f;  // How much to deviate from default 90
	float Speed = bWarpActive ? 2.0f : 4.0f;  // Faster return on deactivate

	CurrentFOVAdjust = FMath::FInterpTo(CurrentFOVAdjust, DesiredAdjust, DeltaTime, Speed);

	ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(this, 0);
	if (!PlayerChar) return;

	UCameraComponent* Camera = PlayerChar->FindComponentByClass<UCameraComponent>();
	if (Camera)
	{
		Camera->SetFieldOfView(90.f + CurrentFOVAdjust);
	}
}
