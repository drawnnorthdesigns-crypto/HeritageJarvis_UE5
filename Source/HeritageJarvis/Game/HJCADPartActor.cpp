#include "HJCADPartActor.h"
#include "Core/HJMeshLoader.h"
#include "Core/HJNaniteConverter.h"
#include "ProceduralMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

AHJCADPartActor::AHJCADPartActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Root scene component
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// Procedural mesh for CAD geometry
	PartMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("PartMesh"));
	PartMesh->SetupAttachment(RootComponent);
	PartMesh->CastShadow = true;

	// Mesh loader component
	MeshLoader = CreateDefaultSubobject<UHJMeshLoader>(TEXT("MeshLoader"));
	MeshLoader->TargetMeshComponent = PartMesh;
	MeshLoader->bEnableCollision = true;
	MeshLoader->bPreferBinaryMode = true;

	// Nanite converter — auto-converts large meshes to StaticMesh+Nanite
	NaniteConverter = CreateDefaultSubobject<UHJNaniteConverter>(TEXT("NaniteConverter"));

	// Accent light — golden glow around the part
	PartLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("PartLight"));
	PartLight->SetupAttachment(RootComponent);
	PartLight->SetLightColor(FLinearColor(0.83f, 0.66f, 0.22f));
	PartLight->SetIntensity(5000.f);
	PartLight->SetAttenuationRadius(300.f);
	PartLight->CastShadows = false;
	PartLight->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
}

void AHJCADPartActor::BeginPlay()
{
	Super::BeginPlay();

	// Bind mesh load callback
	MeshLoader->OnMeshLoaded.AddDynamic(this, &AHJCADPartActor::OnMeshLoadComplete);

	if (bAutoLoad && !ProjectId.IsEmpty() && !ComponentName.IsEmpty())
	{
		LoadFromPipeline();
	}
}

void AHJCADPartActor::OnInteract_Implementation(APlayerController* Interactor)
{
	if (ProjectId.IsEmpty()) return;

	// Open the 3D viewer in CEF dashboard for this specific model
	FString ViewerUrl = FString::Printf(
		TEXT("http://127.0.0.1:5000/viewer/%s?file=%s"),
		*ProjectId, *ComponentName);

	UE_LOG(LogTemp, Log, TEXT("HJCADPartActor: Interact — opening viewer for %s/%s"),
		*ProjectId, *ComponentName);

	// Navigate CEF to viewer URL
	// (The TartariaGameMode will handle the actual CEF navigation)
}

FString AHJCADPartActor::GetInteractPrompt_Implementation() const
{
	return FString::Printf(TEXT("Inspect %s"), *DisplayName);
}

void AHJCADPartActor::LoadFromPipeline()
{
	if (ProjectId.IsEmpty() || ComponentName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("HJCADPartActor: Cannot load — ProjectId or ComponentName empty"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("HJCADPartActor: Loading mesh for %s/%s"),
		*ProjectId, *ComponentName);

	MeshLoader->LoadMeshFromProject(ProjectId, ComponentName);
}

void AHJCADPartActor::OnMeshLoadComplete(bool bSuccess, const FString& StatusMessage)
{
	bMeshLoaded = bSuccess;

	if (bSuccess)
	{
		TriangleCount = MeshLoader->LoadedTriangleCount;

		// Auto-scale: fit mesh within MaxWorldSize bounding sphere
		if (PartMesh)
		{
			FBoxSphereBounds Bounds = PartMesh->CalcBounds(PartMesh->GetComponentTransform());
			float CurrentSize = Bounds.SphereRadius * 2.0f;
			if (CurrentSize > 0.01f)
			{
				float Scale = MaxWorldSize / CurrentSize;
				PartMesh->SetRelativeScale3D(FVector(Scale));

				// Reposition light above the scaled part
				PartLight->SetRelativeLocation(FVector(0.f, 0.f, MaxWorldSize * 0.6f));
			}
		}

		// Apply Tartarian bronze material
		UMaterialInstanceDynamic* DynMat = PartMesh->CreateAndSetMaterialInstanceDynamic(0);
		if (DynMat)
		{
			DynMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.55f, 0.38f, 0.18f));
		}

		UE_LOG(LogTemp, Log, TEXT("HJCADPartActor: Mesh loaded — %d tris, %s"),
			TriangleCount, *StatusMessage);

		// Attempt Nanite conversion for large meshes
		if (NaniteConverter && PartMesh)
		{
			UStaticMeshComponent* NaniteMesh = NaniteConverter->TryAutoConvert(PartMesh);
			if (NaniteMesh)
			{
				bNaniteConverted = true;
				UE_LOG(LogTemp, Log,
					TEXT("HJCADPartActor: Nanite conversion complete — %d tris in %.1f ms"),
					NaniteConverter->LastResult.TriangleCount,
					NaniteConverter->LastResult.ConversionTimeMs);
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("HJCADPartActor: Mesh load failed — %s"), *StatusMessage);
	}

	OnPartLoaded.Broadcast(bSuccess);
}
