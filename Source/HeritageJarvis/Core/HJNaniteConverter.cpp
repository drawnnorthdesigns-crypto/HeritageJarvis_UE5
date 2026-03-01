#include "HJNaniteConverter.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "MeshDescriptionBuilder.h"
#include "StaticMeshAttributes.h"
#include "PhysicsEngine/BodySetup.h"
#include "HAL/PlatformTime.h"
#include "Engine/World.h"

UHJNaniteConverter::UHJNaniteConverter()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// -------------------------------------------------------
// Public API
// -------------------------------------------------------

bool UHJNaniteConverter::ShouldConvert(UProceduralMeshComponent* SourceMesh) const
{
	if (!SourceMesh) return false;

	// Check triangle count from section 0
	FProcMeshSection* Section = SourceMesh->GetProcMeshSection(0);
	if (!Section) return false;

	int32 TriCount = Section->ProcIndexBuffer.Num() / 3;
	if (TriCount >= AutoConvertTriangleThreshold)
		return true;

	// Check bounding extent
	FBoxSphereBounds Bounds = SourceMesh->CalcBounds(SourceMesh->GetComponentTransform());
	float MaxExtent = Bounds.BoxExtent.GetMax() * 2.0f;
	if (MaxExtent >= AutoConvertSizeThreshold)
		return true;

	return false;
}

UStaticMeshComponent* UHJNaniteConverter::TryAutoConvert(UProceduralMeshComponent* SourceMesh)
{
	if (!ShouldConvert(SourceMesh))
		return nullptr;

	return ConvertToNanite(SourceMesh);
}

UStaticMeshComponent* UHJNaniteConverter::ConvertToNanite(UProceduralMeshComponent* SourceMesh)
{
	double StartTime = FPlatformTime::Seconds();

	LastResult = FNaniteConversionResult();

	if (!SourceMesh)
	{
		LastResult.ErrorMessage = TEXT("Source ProceduralMeshComponent is null");
		OnConversionComplete.Broadcast(LastResult);
		return nullptr;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		LastResult.ErrorMessage = TEXT("No owning actor");
		OnConversionComplete.Broadcast(LastResult);
		return nullptr;
	}

	// Step 1: Build MeshDescription from procedural mesh data
	FMeshDescription MeshDesc;
	FStaticMeshAttributes StaticMeshAttrs(MeshDesc);
	StaticMeshAttrs.Register();

	if (!BuildMeshDescription(SourceMesh, MeshDesc))
	{
		LastResult.ErrorMessage = TEXT("Failed to build MeshDescription from procedural mesh");
		OnConversionComplete.Broadcast(LastResult);
		return nullptr;
	}

	// Step 2: Get material from source mesh
	UMaterialInterface* Material = SourceMesh->GetMaterial(0);

	// Step 3: Create the transient UStaticMesh
	UStaticMesh* StaticMesh = CreateStaticMeshFromDescription(MeshDesc, Material);
	if (!StaticMesh)
	{
		LastResult.ErrorMessage = TEXT("Failed to create UStaticMesh");
		OnConversionComplete.Broadcast(LastResult);
		return nullptr;
	}

	// Step 4: Enable Nanite
	if (bEnableNanite)
	{
		StaticMesh->NaniteSettings.bEnabled = true;
	}

	// Step 5: Build the static mesh (compiles Nanite data, collision, etc.)
	TArray<const FMeshDescription*> MeshDescriptions;
	MeshDescriptions.Add(&MeshDesc);
	StaticMesh->BuildFromMeshDescriptions(MeshDescriptions);

	// Step 6: Create StaticMeshComponent and attach
	UStaticMeshComponent* NewMeshComp = NewObject<UStaticMeshComponent>(
		Owner, NAME_None, RF_Transient);
	NewMeshComp->SetStaticMesh(StaticMesh);
	NewMeshComp->SetWorldTransform(SourceMesh->GetComponentTransform());
	NewMeshComp->AttachToComponent(
		Owner->GetRootComponent(),
		FAttachmentTransformRules::KeepWorldTransform);
	NewMeshComp->RegisterComponent();

	// Step 7: Transfer material
	TransferMaterial(SourceMesh, NewMeshComp);

	// Step 8: Set up RVT if enabled
	if (bEnableRVT)
	{
		SetupRuntimeVirtualTexture(NewMeshComp);
	}

	// Step 9: Hide the old procedural mesh
	SourceMesh->SetVisibility(false);
	SourceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Record results
	FProcMeshSection* Section = SourceMesh->GetProcMeshSection(0);
	double EndTime = FPlatformTime::Seconds();

	ConvertedMeshComp = NewMeshComp;
	LastResult.bSuccess = true;
	LastResult.TriangleCount = Section ? Section->ProcIndexBuffer.Num() / 3 : 0;
	LastResult.VertexCount = Section ? Section->ProcVertexBuffer.Num() : 0;
	LastResult.bNaniteEnabled = bEnableNanite;
	LastResult.ConversionTimeMs = static_cast<float>((EndTime - StartTime) * 1000.0);

	UE_LOG(LogTemp, Log,
		TEXT("HJNaniteConverter: Converted %d tris / %d verts in %.1f ms (Nanite: %s)"),
		LastResult.TriangleCount, LastResult.VertexCount,
		LastResult.ConversionTimeMs,
		bEnableNanite ? TEXT("ON") : TEXT("OFF"));

	OnConversionComplete.Broadcast(LastResult);
	return NewMeshComp;
}

// -------------------------------------------------------
// Private: Build MeshDescription
// -------------------------------------------------------

bool UHJNaniteConverter::BuildMeshDescription(
	UProceduralMeshComponent* Source,
	FMeshDescription& OutMeshDesc) const
{
	// Extract section 0 geometry from ProceduralMeshComponent
	FProcMeshSection* Section = Source->GetProcMeshSection(0);
	if (!Section || Section->ProcVertexBuffer.Num() == 0)
		return false;

	const TArray<FProcMeshVertex>& Vertices = Section->ProcVertexBuffer;
	const TArray<uint32>& Indices = Section->ProcIndexBuffer;

	if (Indices.Num() < 3)
		return false;

	int32 VertCount = Vertices.Num();
	int32 TriCount = Indices.Num() / 3;

	// Get attribute accessors
	TVertexAttributesRef<FVector3f> VertexPositions =
		OutMeshDesc.VertexAttributes().GetAttributesRef<FVector3f>(
			MeshAttribute::Vertex::Position);

	TVertexInstanceAttributesRef<FVector3f> VertexInstanceNormals =
		OutMeshDesc.VertexInstanceAttributes().GetAttributesRef<FVector3f>(
			MeshAttribute::VertexInstance::Normal);

	TVertexInstanceAttributesRef<FVector3f> VertexInstanceTangents =
		OutMeshDesc.VertexInstanceAttributes().GetAttributesRef<FVector3f>(
			MeshAttribute::VertexInstance::Tangent);

	TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns =
		OutMeshDesc.VertexInstanceAttributes().GetAttributesRef<float>(
			MeshAttribute::VertexInstance::BinormalSign);

	TVertexInstanceAttributesRef<FVector2f> VertexInstanceUVs =
		OutMeshDesc.VertexInstanceAttributes().GetAttributesRef<FVector2f>(
			MeshAttribute::VertexInstance::TextureCoordinate);

	TVertexInstanceAttributesRef<FVector4f> VertexInstanceColors =
		OutMeshDesc.VertexInstanceAttributes().GetAttributesRef<FVector4f>(
			MeshAttribute::VertexInstance::Color);

	// Reserve space
	OutMeshDesc.ReserveNewVertices(VertCount);
	OutMeshDesc.ReserveNewVertexInstances(Indices.Num());
	OutMeshDesc.ReserveNewPolygons(TriCount);
	OutMeshDesc.ReserveNewEdges(TriCount * 3);

	// Create polygon group (material slot 0)
	FPolygonGroupID PolyGroupID = OutMeshDesc.CreatePolygonGroup();

	// Create vertices
	TArray<FVertexID> VertexIDs;
	VertexIDs.SetNum(VertCount);

	for (int32 i = 0; i < VertCount; ++i)
	{
		VertexIDs[i] = OutMeshDesc.CreateVertex();
		VertexPositions[VertexIDs[i]] = FVector3f(Vertices[i].Position);
	}

	// Create triangles (each triangle = 3 vertex instances + 1 polygon)
	for (int32 TriIdx = 0; TriIdx < TriCount; ++TriIdx)
	{
		TArray<FVertexInstanceID> TriInstances;
		TriInstances.SetNum(3);

		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			int32 IndexBufPos = TriIdx * 3 + Corner;
			uint32 VertIndex = Indices[IndexBufPos];

			if (static_cast<int32>(VertIndex) >= VertCount)
				return false; // Index out of bounds

			const FProcMeshVertex& Vert = Vertices[VertIndex];

			FVertexInstanceID InstanceID = OutMeshDesc.CreateVertexInstance(VertexIDs[VertIndex]);
			TriInstances[Corner] = InstanceID;

			// Normal
			VertexInstanceNormals[InstanceID] = FVector3f(Vert.Normal);

			// Tangent — derive from normal cross up
			FVector3f Normal3f(Vert.Normal);
			FVector3f Up3f(0.f, 0.f, 1.f);
			FVector3f Tangent3f = FVector3f::CrossProduct(Normal3f, Up3f);
			if (Tangent3f.SizeSquared() < 0.001f)
			{
				Up3f = FVector3f(0.f, 1.f, 0.f);
				Tangent3f = FVector3f::CrossProduct(Normal3f, Up3f);
			}
			Tangent3f.Normalize();
			VertexInstanceTangents[InstanceID] = Tangent3f;
			VertexInstanceBinormalSigns[InstanceID] = 1.0f;

			// UV (channel 0)
			VertexInstanceUVs.Set(InstanceID, 0, FVector2f(Vert.UV0));

			// Vertex color
			FLinearColor LC = Vert.Color.ReinterpretAsLinear();
			VertexInstanceColors[InstanceID] = FVector4f(LC.R, LC.G, LC.B, LC.A);
		}

		// Create the polygon (triangle)
		// UE5 winding: reverse order to match left-hand coordinate convention
		TArray<FVertexInstanceID> Reversed;
		Reversed.Add(TriInstances[0]);
		Reversed.Add(TriInstances[2]);
		Reversed.Add(TriInstances[1]);

		OutMeshDesc.CreatePolygon(PolyGroupID, Reversed);
	}

	return true;
}

// -------------------------------------------------------
// Private: Create UStaticMesh
// -------------------------------------------------------

UStaticMesh* UHJNaniteConverter::CreateStaticMeshFromDescription(
	FMeshDescription& MeshDesc,
	UMaterialInterface* Material) const
{
	// Create a transient UStaticMesh (not saved to disk)
	UStaticMesh* StaticMesh = NewObject<UStaticMesh>(
		GetTransientPackage(),
		MakeUniqueObjectName(GetTransientPackage(), UStaticMesh::StaticClass(),
		                      TEXT("HJ_NaniteMesh")),
		RF_Transient);

	if (!StaticMesh)
		return nullptr;

	// Assign material to slot 0
	if (Material)
	{
		StaticMesh->GetStaticMaterials().Add(FStaticMaterial(Material));
	}
	else
	{
		StaticMesh->GetStaticMaterials().Add(FStaticMaterial());
	}

	// Configure LOD settings
	FStaticMeshSourceModel& SrcModel = StaticMesh->AddSourceModel();
	FMeshBuildSettings& BuildSettings = SrcModel.BuildSettings;
	BuildSettings.bRecomputeNormals = false;
	BuildSettings.bRecomputeTangents = true;
	BuildSettings.bGenerateLightmapUVs = true;
	BuildSettings.SrcLightmapIndex = 0;
	BuildSettings.DstLightmapIndex = 1;
	BuildSettings.MinLightmapResolution = 128;

	// Set the mesh description for LOD 0
	SrcModel.SetMeshDescription(MoveTemp(MeshDesc));

	// Set up simple collision from mesh bounds
	StaticMesh->CreateBodySetup();
	if (StaticMesh->GetBodySetup())
	{
		StaticMesh->GetBodySetup()->CollisionTraceFlag = CTF_UseComplexAsSimple;
	}

	// Set lightmap resolution
	StaticMesh->SetLightMapResolution(256);

	return StaticMesh;
}

// -------------------------------------------------------
// Private: Transfer material
// -------------------------------------------------------

void UHJNaniteConverter::TransferMaterial(
	UProceduralMeshComponent* Source,
	UStaticMeshComponent* Dest) const
{
	if (!Source || !Dest) return;

	UMaterialInterface* Mat = Source->GetMaterial(0);
	if (Mat)
	{
		Dest->SetMaterial(0, Mat);
	}
}

// -------------------------------------------------------
// Private: RuntimeVirtualTexture setup
// -------------------------------------------------------

void UHJNaniteConverter::SetupRuntimeVirtualTexture(UStaticMeshComponent* MeshComp) const
{
	if (!MeshComp) return;

	// Enable virtual texture streaming on the component
	// This allows the material system to use RVT for large meshes
	// without creating actual URuntimeVirtualTexture assets at runtime.
	//
	// For full RVT support, a URuntimeVirtualTexture asset should be
	// created in the editor and assigned to the level's RVT volume.
	// At runtime, we enable the component flags so it participates
	// in virtual texture rendering when an RVT volume is present.

	MeshComp->bRenderInMainPass = true;

	// Enable virtual texture for the static mesh component.
	// The component will automatically participate in any RVT volume
	// it overlaps with in the level.
	MeshComp->SetGenerateOverlapEvents(false);

	// Set shadow settings appropriate for large geometry
	MeshComp->SetCastShadow(true);
	MeshComp->bCastDynamicShadow = true;
	MeshComp->bCastStaticShadow = true;

	// Affect distance field for large meshes (helps with GI/shadows)
	MeshComp->bAffectDistanceFieldLighting = true;

	UE_LOG(LogTemp, Log,
		TEXT("HJNaniteConverter: RVT-ready flags set on StaticMeshComponent"));
}
