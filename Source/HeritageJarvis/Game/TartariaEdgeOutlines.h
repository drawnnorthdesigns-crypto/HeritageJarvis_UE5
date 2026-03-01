#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"

/**
 * TartariaEdgeOutlines -- Shared utility for generating gold emissive edge
 * outlines on compound-primitive actors (buildings, NPCs, landmarks).
 *
 * Uses the "bounding-box wireframe" technique: for each source mesh component,
 * compute its local bounding box and spawn 12 thin cube meshes along the 12
 * edges.  Each strip gets an emissive gold material so the edges glow,
 * making primitives read as designed architectural objects.
 */
struct FTartariaEdgeOutlines
{
	/**
	 * Generate 12 emissive edge strips around the bounding box of SourceMesh.
	 *
	 * @param Owner          The actor that owns the new mesh components
	 * @param SourceMesh     The mesh component whose bounding box to outline
	 * @param OutEdges       Appended with the newly created edge strip components
	 * @param EdgeColor      Emissive edge color (linear, pre-multiplied by EmissiveStrength)
	 * @param EdgeWidth      Width of each edge strip in cm (default 1.5)
	 * @param EmissiveStrength  Multiplier for emissive glow (default 3.0)
	 */
	static void GenerateBoundingBoxEdges(
		AActor* Owner,
		UStaticMeshComponent* SourceMesh,
		TArray<UStaticMeshComponent*>& OutEdges,
		const FLinearColor& EdgeColor = FLinearColor(0.78f, 0.63f, 0.3f),
		float EdgeWidth = 1.5f,
		float EmissiveStrength = 3.f)
	{
		if (!Owner || !SourceMesh) return;

		// Load the cube mesh used for all edge strips
		static UStaticMesh* CubeMesh = nullptr;
		if (!CubeMesh)
		{
			CubeMesh = LoadObject<UStaticMesh>(
				nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		}
		if (!CubeMesh) return;

		// Get the local bounding box of the source mesh
		FBoxSphereBounds LocalBounds = SourceMesh->CalcLocalBounds();
		FVector Min = LocalBounds.Origin - LocalBounds.BoxExtent;
		FVector Max = LocalBounds.Origin + LocalBounds.BoxExtent;

		// Apply the source mesh's relative scale to get the actual visual extents
		FVector MeshScale = SourceMesh->GetRelativeScale3D();
		Min *= MeshScale;
		Max *= MeshScale;

		// Edge strip thickness: EdgeWidth cm in world, but we need to express
		// it relative to the cube's 100 UU native size => scale = EdgeWidth / 100
		const float StripScale = EdgeWidth / 100.f;

		// Corner positions
		const float X0 = Min.X, X1 = Max.X;
		const float Y0 = Min.Y, Y1 = Max.Y;
		const float Z0 = Min.Z, Z1 = Max.Z;

		// Edge lengths
		const float LenX = X1 - X0;
		const float LenY = Y1 - Y0;
		const float LenZ = Z1 - Z0;

		// Scale factors for edges along each axis (length / 100 UU cube)
		const float ScaleX = LenX / 100.f;
		const float ScaleY = LenY / 100.f;
		const float ScaleZ = LenZ / 100.f;

		// Emissive color = EdgeColor * EmissiveStrength
		const FLinearColor EmissiveValue = EdgeColor * EmissiveStrength;

		// Edge definitions: { midpoint position, scale3D }
		// 12 edges total: 4 along X, 4 along Y, 4 along Z
		struct FEdgeDef
		{
			FVector Position;
			FVector Scale;
		};

		const float MidX = (X0 + X1) * 0.5f;
		const float MidY = (Y0 + Y1) * 0.5f;
		const float MidZ = (Z0 + Z1) * 0.5f;

		TArray<FEdgeDef> Edges;
		Edges.Reserve(12);

		// 4 edges along X axis (bottom-front, bottom-back, top-front, top-back)
		Edges.Add({ FVector(MidX, Y0, Z0), FVector(ScaleX, StripScale, StripScale) });
		Edges.Add({ FVector(MidX, Y1, Z0), FVector(ScaleX, StripScale, StripScale) });
		Edges.Add({ FVector(MidX, Y0, Z1), FVector(ScaleX, StripScale, StripScale) });
		Edges.Add({ FVector(MidX, Y1, Z1), FVector(ScaleX, StripScale, StripScale) });

		// 4 edges along Y axis (bottom-left, bottom-right, top-left, top-right)
		Edges.Add({ FVector(X0, MidY, Z0), FVector(StripScale, ScaleY, StripScale) });
		Edges.Add({ FVector(X1, MidY, Z0), FVector(StripScale, ScaleY, StripScale) });
		Edges.Add({ FVector(X0, MidY, Z1), FVector(StripScale, ScaleY, StripScale) });
		Edges.Add({ FVector(X1, MidY, Z1), FVector(StripScale, ScaleY, StripScale) });

		// 4 edges along Z axis (vertical corners)
		Edges.Add({ FVector(X0, Y0, MidZ), FVector(StripScale, StripScale, ScaleZ) });
		Edges.Add({ FVector(X1, Y0, MidZ), FVector(StripScale, StripScale, ScaleZ) });
		Edges.Add({ FVector(X0, Y1, MidZ), FVector(StripScale, StripScale, ScaleZ) });
		Edges.Add({ FVector(X1, Y1, MidZ), FVector(StripScale, StripScale, ScaleZ) });

		// Counter for unique component names
		static int32 GlobalEdgeCounter = 0;

		for (int32 i = 0; i < Edges.Num(); ++i)
		{
			const FEdgeDef& Edge = Edges[i];

			FName CompName = *FString::Printf(TEXT("EdgeOutline_%d_%d"),
				GlobalEdgeCounter++, i);

			UStaticMeshComponent* Strip = NewObject<UStaticMeshComponent>(Owner, CompName);
			if (!Strip) continue;

			Strip->SetStaticMesh(CubeMesh);
			Strip->SetupAttachment(SourceMesh);
			Strip->RegisterComponent();

			Strip->SetRelativeLocation(Edge.Position);
			Strip->SetRelativeScale3D(Edge.Scale);
			Strip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Strip->SetCastShadow(false);

			// Apply emissive gold material
			UMaterialInstanceDynamic* DynMat =
				Strip->CreateAndSetMaterialInstanceDynamic(0);
			if (DynMat)
			{
				DynMat->SetVectorParameterValue(TEXT("Color"), EdgeColor);
				DynMat->SetVectorParameterValue(TEXT("EmissiveColor"), EmissiveValue);
				DynMat->SetScalarParameterValue(TEXT("Emissive"), EmissiveStrength);
			}

			OutEdges.Add(Strip);
		}
	}
};
