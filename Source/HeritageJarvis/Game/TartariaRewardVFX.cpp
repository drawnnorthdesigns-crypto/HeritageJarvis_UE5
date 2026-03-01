#include "TartariaRewardVFX.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

ATartariaRewardVFX::ATartariaRewardVFX()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	VFXRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VFXRoot"));
	RootComponent = VFXRoot;

	FlashLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("Flash"));
	FlashLight->SetupAttachment(VFXRoot);
	FlashLight->SetRelativeLocation(FVector(0.f, 0.f, 50.f));
	FlashLight->SetIntensity(0.f);
	FlashLight->SetAttenuationRadius(1000.f);
	FlashLight->CastShadows = false;
}

ATartariaRewardVFX* ATartariaRewardVFX::SpawnRewardVFX(
	const UObject* WorldContextObject, FVector Location,
	ERewardVFXType Type, float Duration)
{
	if (!WorldContextObject) return nullptr;
	UWorld* World = WorldContextObject->GetWorld();
	if (!World) return nullptr;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ATartariaRewardVFX* VFX = World->SpawnActor<ATartariaRewardVFX>(
		ATartariaRewardVFX::StaticClass(), Location, FRotator::ZeroRotator, Params);

	if (VFX)
		VFX->PlayEffect(Type, Duration);

	return VFX;
}

void ATartariaRewardVFX::PlayEffect(ERewardVFXType Type, float Duration)
{
	EffectType = Type;
	EffectDuration = Duration;
	ElapsedTime = 0.f;
	bPlaying = true;

	switch (Type)
	{
	case ERewardVFXType::BuildingUpgrade:
		FlashLight->SetLightColor(FLinearColor(0.9f, 0.75f, 0.2f));
		BuildShockwave();
		break;
	case ERewardVFXType::CombatVictory:
		FlashLight->SetLightColor(FLinearColor(0.8f, 0.2f, 0.1f));
		BuildShatter();
		break;
	case ERewardVFXType::QuestComplete:
	case ERewardVFXType::LevelUp:
		FlashLight->SetLightColor(FLinearColor(0.3f, 0.6f, 1.0f));
		BuildPillar();
		break;
	case ERewardVFXType::Materialization:
		FlashLight->SetLightColor(FLinearColor(0.5f, 0.8f, 0.4f));
		BuildSwirl();
		break;
	}

	// Initial flash burst
	FlashLight->SetIntensity(20000.f);

	SetActorTickEnabled(true);

	UE_LOG(LogTemp, Log, TEXT("TartariaRewardVFX: Playing %d at %s (%.1fs)"),
		static_cast<int32>(Type), *GetActorLocation().ToString(), Duration);
}

void ATartariaRewardVFX::BuildShockwave()
{
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!CubeMesh) return;

	// 16 ring segments expanding outward
	const int32 Count = 16;
	for (int32 i = 0; i < Count; i++)
	{
		float Angle = (static_cast<float>(i) / Count) * 360.f;
		float AngleRad = FMath::DegreesToRadians(Angle);

		UStaticMeshComponent* Seg = NewObject<UStaticMeshComponent>(this,
			*FString::Printf(TEXT("SW_%d"), i));
		Seg->SetupAttachment(VFXRoot);
		Seg->SetStaticMesh(CubeMesh);
		Seg->SetRelativeLocation(FVector(0.f, 0.f, 5.f));
		Seg->SetRelativeScale3D(FVector(0.2f, 0.05f, 0.02f));
		Seg->SetRelativeRotation(FRotator(0.f, Angle, 0.f));
		Seg->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Seg->CastShadow = false;
		Seg->RegisterComponent();

		UMaterialInstanceDynamic* Mat = Seg->CreateAndSetMaterialInstanceDynamic(0);
		if (Mat)
		{
			Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.9f, 0.75f, 0.2f));
			Mat->SetScalarParameterValue(TEXT("Emissive"), 5.0f);
		}

		ParticleMeshes.Add(Seg);
		ParticleVelocities.Add(FVector(
			FMath::Cos(AngleRad) * 400.f,
			FMath::Sin(AngleRad) * 400.f,
			50.f + FMath::FRandRange(0.f, 30.f)
		));
	}

	// 8 rising sparkle cubes
	for (int32 i = 0; i < 8; i++)
	{
		UStaticMeshComponent* Spark = NewObject<UStaticMeshComponent>(this,
			*FString::Printf(TEXT("Spark_%d"), i));
		Spark->SetupAttachment(VFXRoot);
		Spark->SetStaticMesh(CubeMesh);
		Spark->SetRelativeLocation(FVector(
			FMath::FRandRange(-30.f, 30.f),
			FMath::FRandRange(-30.f, 30.f),
			10.f
		));
		Spark->SetRelativeScale3D(FVector(0.03f, 0.03f, 0.03f));
		Spark->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Spark->CastShadow = false;
		Spark->RegisterComponent();

		UMaterialInstanceDynamic* Mat = Spark->CreateAndSetMaterialInstanceDynamic(0);
		if (Mat)
		{
			Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.9f, 0.4f));
			Mat->SetScalarParameterValue(TEXT("Emissive"), 8.0f);
		}

		ParticleMeshes.Add(Spark);
		ParticleVelocities.Add(FVector(
			FMath::FRandRange(-20.f, 20.f),
			FMath::FRandRange(-20.f, 20.f),
			200.f + FMath::FRandRange(0.f, 100.f)
		));
	}
}

void ATartariaRewardVFX::BuildShatter()
{
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!CubeMesh) return;

	// 20 shattering cubes exploding outward
	for (int32 i = 0; i < 20; i++)
	{
		UStaticMeshComponent* Shard = NewObject<UStaticMeshComponent>(this,
			*FString::Printf(TEXT("Shard_%d"), i));
		Shard->SetupAttachment(VFXRoot);
		Shard->SetStaticMesh(CubeMesh);
		Shard->SetRelativeLocation(FVector(0.f, 0.f, 50.f));

		float Scale = FMath::FRandRange(0.03f, 0.08f);
		Shard->SetRelativeScale3D(FVector(Scale, Scale, Scale));
		Shard->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Shard->CastShadow = false;
		Shard->RegisterComponent();

		UMaterialInstanceDynamic* Mat = Shard->CreateAndSetMaterialInstanceDynamic(0);
		if (Mat)
		{
			// Alternate between red and orange
			FLinearColor ShardColor = (i % 2 == 0)
				? FLinearColor(0.8f, 0.2f, 0.1f)
				: FLinearColor(0.9f, 0.5f, 0.1f);
			Mat->SetVectorParameterValue(TEXT("Color"), ShardColor);
			Mat->SetScalarParameterValue(TEXT("Emissive"), 3.0f);
		}

		ParticleMeshes.Add(Shard);

		// Explosion velocity — outward in all directions
		FVector Dir = FMath::VRand();
		Dir.Z = FMath::Abs(Dir.Z);  // Bias upward
		ParticleVelocities.Add(Dir * FMath::FRandRange(200.f, 500.f));
	}

	// 6 loot orbs spiraling upward
	UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (!SphereMesh) return;

	for (int32 i = 0; i < 6; i++)
	{
		UStaticMeshComponent* Orb = NewObject<UStaticMeshComponent>(this,
			*FString::Printf(TEXT("Loot_%d"), i));
		Orb->SetupAttachment(VFXRoot);
		Orb->SetStaticMesh(SphereMesh);
		Orb->SetRelativeLocation(FVector(0.f, 0.f, 30.f));
		Orb->SetRelativeScale3D(FVector(0.06f, 0.06f, 0.06f));
		Orb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Orb->CastShadow = false;
		Orb->RegisterComponent();

		UMaterialInstanceDynamic* Mat = Orb->CreateAndSetMaterialInstanceDynamic(0);
		if (Mat)
		{
			Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.9f, 0.8f, 0.2f));
			Mat->SetScalarParameterValue(TEXT("Emissive"), 6.0f);
		}

		ParticleMeshes.Add(Orb);
		// Spiral velocity (will be overridden in animate for spiral motion)
		ParticleVelocities.Add(FVector(0.f, 0.f, 100.f));
	}
}

void ATartariaRewardVFX::BuildPillar()
{
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!CubeMesh) return;

	// Central pillar beam — tall thin cube that grows
	UStaticMeshComponent* Beam = NewObject<UStaticMeshComponent>(this, TEXT("Beam"));
	Beam->SetupAttachment(VFXRoot);
	Beam->SetStaticMesh(CubeMesh);
	Beam->SetRelativeLocation(FVector(0.f, 0.f, 0.f));
	Beam->SetRelativeScale3D(FVector(0.05f, 0.05f, 0.01f));  // Starts tiny
	Beam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beam->CastShadow = false;
	Beam->RegisterComponent();

	UMaterialInstanceDynamic* Mat = Beam->CreateAndSetMaterialInstanceDynamic(0);
	if (Mat)
	{
		Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.4f, 0.7f, 1.0f));
		Mat->SetScalarParameterValue(TEXT("Emissive"), 8.0f);
	}

	ParticleMeshes.Add(Beam);
	ParticleVelocities.Add(FVector(0.f, 0.f, 0.f));  // Grows in-place

	// 3 expanding halo rings
	for (int32 i = 0; i < 3; i++)
	{
		float Height = 50.f + i * 80.f;

		// Each ring is 8 cube segments
		for (int32 j = 0; j < 8; j++)
		{
			float Angle = (static_cast<float>(j) / 8.f) * 360.f;

			UStaticMeshComponent* Seg = NewObject<UStaticMeshComponent>(this,
				*FString::Printf(TEXT("Halo_%d_%d"), i, j));
			Seg->SetupAttachment(VFXRoot);
			Seg->SetStaticMesh(CubeMesh);
			Seg->SetRelativeLocation(FVector(0.f, 0.f, Height));
			Seg->SetRelativeRotation(FRotator(0.f, Angle, 0.f));
			Seg->SetRelativeScale3D(FVector(0.1f, 0.02f, 0.01f));
			Seg->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Seg->CastShadow = false;
			Seg->RegisterComponent();

			UMaterialInstanceDynamic* HaloMat = Seg->CreateAndSetMaterialInstanceDynamic(0);
			if (HaloMat)
			{
				float Brightness = 1.0f - i * 0.2f;
				HaloMat->SetVectorParameterValue(TEXT("Color"),
					FLinearColor(0.3f * Brightness, 0.6f * Brightness, 1.0f * Brightness));
				HaloMat->SetScalarParameterValue(TEXT("Emissive"), 5.0f);
			}

			ParticleMeshes.Add(Seg);
			float AngleRad = FMath::DegreesToRadians(Angle);
			ParticleVelocities.Add(FVector(
				FMath::Cos(AngleRad) * (100.f + i * 50.f),
				FMath::Sin(AngleRad) * (100.f + i * 50.f),
				30.f
			));
		}
	}
}

void ATartariaRewardVFX::BuildSwirl()
{
	UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (!SphereMesh) return;

	// 12 orbs converging from outside to center
	for (int32 i = 0; i < 12; i++)
	{
		float Angle = (static_cast<float>(i) / 12.f) * 360.f;
		float AngleRad = FMath::DegreesToRadians(Angle);
		float StartRadius = 200.f;

		UStaticMeshComponent* Orb = NewObject<UStaticMeshComponent>(this,
			*FString::Printf(TEXT("Swirl_%d"), i));
		Orb->SetupAttachment(VFXRoot);
		Orb->SetStaticMesh(SphereMesh);
		Orb->SetRelativeLocation(FVector(
			FMath::Cos(AngleRad) * StartRadius,
			FMath::Sin(AngleRad) * StartRadius,
			50.f + (i % 3) * 20.f
		));
		Orb->SetRelativeScale3D(FVector(0.04f, 0.04f, 0.04f));
		Orb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Orb->CastShadow = false;
		Orb->RegisterComponent();

		UMaterialInstanceDynamic* Mat = Orb->CreateAndSetMaterialInstanceDynamic(0);
		if (Mat)
		{
			FLinearColor OrbColor = (i % 3 == 0)
				? FLinearColor(0.4f, 0.9f, 0.3f)   // Green
				: (i % 3 == 1)
					? FLinearColor(0.3f, 0.5f, 0.9f) // Blue
					: FLinearColor(0.9f, 0.8f, 0.3f); // Gold
			Mat->SetVectorParameterValue(TEXT("Color"), OrbColor);
			Mat->SetScalarParameterValue(TEXT("Emissive"), 4.0f);
		}

		ParticleMeshes.Add(Orb);
		// Velocity toward center (will spiral in animate)
		ParticleVelocities.Add(FVector(
			-FMath::Cos(AngleRad) * 150.f,
			-FMath::Sin(AngleRad) * 150.f,
			20.f
		));
	}
}

void ATartariaRewardVFX::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bPlaying) return;

	ElapsedTime += DeltaTime;
	float Progress = FMath::Clamp(ElapsedTime / EffectDuration, 0.f, 1.f);

	AnimateEffect(DeltaTime);

	// Fade flash light
	float FlashDecay = FMath::Max(0.f, 1.0f - Progress * 3.f);  // Fast fade in first third
	FlashLight->SetIntensity(20000.f * FlashDecay);

	// Fade particles in last quarter
	if (Progress > 0.75f)
	{
		float FadeAlpha = 1.0f - (Progress - 0.75f) * 4.f;
		for (UStaticMeshComponent* Mesh : ParticleMeshes)
		{
			if (Mesh)
			{
				float Scale = FMath::Max(0.001f, Mesh->GetRelativeScale3D().X * (0.95f + FadeAlpha * 0.05f));
				// Don't shrink below minimum
			}
		}
	}

	// Self-destruct when done
	if (Progress >= 1.0f)
	{
		bPlaying = false;
		Destroy();
	}
}

void ATartariaRewardVFX::AnimateEffect(float DeltaTime)
{
	float Progress = FMath::Clamp(ElapsedTime / EffectDuration, 0.f, 1.f);

	switch (EffectType)
	{
	case ERewardVFXType::BuildingUpgrade:
	{
		// Ring segments expand outward, sparkles rise
		for (int32 i = 0; i < ParticleMeshes.Num(); i++)
		{
			if (!ParticleMeshes[i]) continue;
			FVector Loc = ParticleMeshes[i]->GetRelativeLocation();
			Loc += ParticleVelocities[i] * DeltaTime;
			// Gravity on sparkles
			if (i >= 16) ParticleVelocities[i].Z -= 50.f * DeltaTime;
			ParticleMeshes[i]->SetRelativeLocation(Loc);

			// Ring segments rotate
			if (i < 16)
			{
				FRotator Rot = ParticleMeshes[i]->GetRelativeRotation();
				Rot.Yaw += 60.f * DeltaTime;
				ParticleMeshes[i]->SetRelativeRotation(Rot);
			}
		}
		break;
	}
	case ERewardVFXType::CombatVictory:
	{
		// Shards explode outward with gravity, loot orbs spiral
		for (int32 i = 0; i < ParticleMeshes.Num(); i++)
		{
			if (!ParticleMeshes[i]) continue;
			FVector Loc = ParticleMeshes[i]->GetRelativeLocation();

			if (i < 20)  // Shards
			{
				ParticleVelocities[i].Z -= 200.f * DeltaTime;  // Gravity
				Loc += ParticleVelocities[i] * DeltaTime;
				// Tumble rotation
				FRotator Rot = ParticleMeshes[i]->GetRelativeRotation();
				Rot.Yaw += 300.f * DeltaTime;
				Rot.Pitch += 200.f * DeltaTime;
				ParticleMeshes[i]->SetRelativeRotation(Rot);
			}
			else  // Loot orbs — spiral upward
			{
				int32 OrbIdx = i - 20;
				float SpiralAngle = ElapsedTime * 180.f + OrbIdx * 60.f;
				float SpiralRad = FMath::DegreesToRadians(SpiralAngle);
				float Radius = 60.f * (1.0f - Progress * 0.5f);
				Loc.X = FMath::Cos(SpiralRad) * Radius;
				Loc.Y = FMath::Sin(SpiralRad) * Radius;
				Loc.Z += 150.f * DeltaTime;
			}
			ParticleMeshes[i]->SetRelativeLocation(Loc);
		}
		break;
	}
	case ERewardVFXType::QuestComplete:
	case ERewardVFXType::LevelUp:
	{
		// Beam grows upward, halos expand
		if (ParticleMeshes.Num() > 0 && ParticleMeshes[0])
		{
			// Pillar beam grows
			float BeamHeight = FMath::Lerp(0.01f, 3.0f, FMath::Min(Progress * 2.f, 1.f));
			ParticleMeshes[0]->SetRelativeScale3D(FVector(0.05f, 0.05f, BeamHeight));
			ParticleMeshes[0]->SetRelativeLocation(FVector(0.f, 0.f, BeamHeight * 50.f));
		}

		// Halo segments expand outward
		for (int32 i = 1; i < ParticleMeshes.Num(); i++)
		{
			if (!ParticleMeshes[i]) continue;
			FVector Loc = ParticleMeshes[i]->GetRelativeLocation();
			Loc += ParticleVelocities[i] * DeltaTime * (1.0f - Progress * 0.5f);
			ParticleMeshes[i]->SetRelativeLocation(Loc);
		}
		break;
	}
	case ERewardVFXType::Materialization:
	{
		// Orbs spiral inward, then flash at center
		for (int32 i = 0; i < ParticleMeshes.Num(); i++)
		{
			if (!ParticleMeshes[i]) continue;

			float Angle = (static_cast<float>(i) / 12.f) * 360.f;
			float SpiralAngle = Angle + ElapsedTime * 200.f;
			float SpiralRad = FMath::DegreesToRadians(SpiralAngle);

			// Radius shrinks as progress increases
			float Radius = 200.f * (1.0f - Progress);
			float Height = 50.f + (i % 3) * 20.f + Progress * 80.f;

			FVector Loc(
				FMath::Cos(SpiralRad) * Radius,
				FMath::Sin(SpiralRad) * Radius,
				Height
			);
			ParticleMeshes[i]->SetRelativeLocation(Loc);

			// Scale up slightly as they converge
			float Scale = 0.04f + Progress * 0.03f;
			ParticleMeshes[i]->SetRelativeScale3D(FVector(Scale, Scale, Scale));
		}

		// Intensify flash at convergence point
		if (Progress > 0.8f)
		{
			float ConvergeFlash = (Progress - 0.8f) * 5.f;
			FlashLight->SetIntensity(30000.f * ConvergeFlash);
		}
		break;
	}
	}
}
