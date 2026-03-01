#include "HJMeshLoader.h"
#include "HJApiClient.h"
#include "HJGameInstance.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"

UHJMeshLoader::UHJMeshLoader()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHJMeshLoader::BeginPlay()
{
	Super::BeginPlay();

	// Auto-find ProceduralMeshComponent on owning actor
	if (!TargetMeshComponent)
	{
		TargetMeshComponent = GetOwner()->FindComponentByClass<UProceduralMeshComponent>();
	}
}

// ── Public API ─────────────────────────────────────────────────

void UHJMeshLoader::LoadMeshFromProject(const FString& ProjectId, const FString& ComponentName)
{
	if (bIsLoading)
	{
		UE_LOG(LogTemp, Warning, TEXT("HJMeshLoader: Already loading a mesh, ignoring request"));
		return;
	}

	bIsLoading = true;
	CurrentProjectId = ProjectId;
	CurrentComponentName = ComponentName;

	if (bPreferBinaryMode)
	{
		FetchMeshBinary(ProjectId, ComponentName);
	}
	else
	{
		FetchMeshJson(ProjectId, ComponentName);
	}
}

void UHJMeshLoader::LoadMeshFromPath(const FString& StlPath)
{
	if (bIsLoading)
	{
		UE_LOG(LogTemp, Warning, TEXT("HJMeshLoader: Already loading a mesh"));
		return;
	}

	bIsLoading = true;

	// Use optimize endpoint with direct path
	UHJApiClient* Api = GetApiClient();
	if (!Api)
	{
		bIsLoading = false;
		OnMeshLoaded.Broadcast(false, TEXT("ApiClient not available"));
		return;
	}

	TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject());
	Body->SetStringField(TEXT("stl_path"), StlPath);
	Body->SetNumberField(TEXT("target_triangles"), MaxJsonTriangles);
	Body->SetBoolField(TEXT("generate_lods"), false);

	FString BodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	FOnApiResponse Callback;
	TWeakObjectPtr<UHJMeshLoader> WeakThis(this);
	Callback.BindLambda([WeakThis](bool bSuccess, const FString& Response)
	{
		UHJMeshLoader* Self = WeakThis.Get();
		if (!Self) return;

		if (!bSuccess)
		{
			Self->bIsLoading = false;
			Self->OnMeshLoaded.Broadcast(false, TEXT("Optimization request failed"));
			return;
		}

		// After optimization, fetch the JSON mesh
		AsyncTask(ENamedThreads::GameThread, [WeakThis]()
		{
			UHJMeshLoader* Self2 = WeakThis.Get();
			if (!Self2) return;
			Self2->FetchMeshJson(Self2->CurrentProjectId, Self2->CurrentComponentName);
		});
	});

	Api->Post(TEXT("/api/mesh/optimize"), BodyStr, Callback);
}

void UHJMeshLoader::FetchMeshMetadata(const FString& ProjectId, const FString& ComponentName)
{
	UHJApiClient* Api = GetApiClient();
	if (!Api)
	{
		OnMeshMetadata.Broadcast(TEXT("{\"error\":\"ApiClient not available\"}"));
		return;
	}

	FString Endpoint = FString::Printf(TEXT("/api/mesh/metadata/%s/%s"), *ProjectId, *ComponentName);

	FOnApiResponse Callback;
	TWeakObjectPtr<UHJMeshLoader> WeakThis(this);
	Callback.BindLambda([WeakThis](bool bSuccess, const FString& Response)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, bSuccess, Response]()
		{
			UHJMeshLoader* Self = WeakThis.Get();
			if (!Self) return;
			Self->OnMeshMetadata.Broadcast(Response);
		});
	});

	Api->Get(Endpoint, Callback);
}

void UHJMeshLoader::SetLOD(const FString& NewLOD)
{
	LODLevel = NewLOD;
	// Reload if we have a current mesh
	if (!CurrentProjectId.IsEmpty() && !CurrentComponentName.IsEmpty())
	{
		ClearMesh();
		LoadMeshFromProject(CurrentProjectId, CurrentComponentName);
	}
}

void UHJMeshLoader::ClearMesh()
{
	UProceduralMeshComponent* MeshComp = GetOrCreateMeshComponent();
	if (MeshComp)
	{
		MeshComp->ClearAllMeshSections();
	}
	LoadedTriangleCount = 0;
	LoadedVertexCount = 0;
	bIsLoading = false;
}

void UHJMeshLoader::CheckPipelineAvailable()
{
	UHJApiClient* Api = GetApiClient();
	if (!Api)
	{
		OnMeshLoaded.Broadcast(false, TEXT("ApiClient not available"));
		return;
	}

	FOnApiResponse Callback;
	TWeakObjectPtr<UHJMeshLoader> WeakThis(this);
	Callback.BindLambda([WeakThis](bool bSuccess, const FString& Response)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, bSuccess, Response]()
		{
			UHJMeshLoader* Self = WeakThis.Get();
			if (!Self) return;
			Self->OnMeshMetadata.Broadcast(Response);
		});
	});

	Api->Get(TEXT("/api/mesh/available"), Callback);
}

// ── Private Helpers ────────────────────────────────────────────

UProceduralMeshComponent* UHJMeshLoader::GetOrCreateMeshComponent()
{
	if (TargetMeshComponent) return TargetMeshComponent;

	AActor* Owner = GetOwner();
	if (!Owner) return nullptr;

	TargetMeshComponent = Owner->FindComponentByClass<UProceduralMeshComponent>();
	if (!TargetMeshComponent)
	{
		TargetMeshComponent = NewObject<UProceduralMeshComponent>(Owner, TEXT("HJProcMesh"));
		TargetMeshComponent->RegisterComponent();
		TargetMeshComponent->AttachToComponent(
			Owner->GetRootComponent(),
			FAttachmentTransformRules::KeepRelativeTransform);
	}
	return TargetMeshComponent;
}

UHJApiClient* UHJMeshLoader::GetApiClient() const
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;

	UHJGameInstance* GI = Cast<UHJGameInstance>(World->GetGameInstance());
	if (!GI) return nullptr;

	return GI->ApiClient;
}

void UHJMeshLoader::FetchMeshJson(const FString& ProjectId, const FString& ComponentName)
{
	UHJApiClient* Api = GetApiClient();
	if (!Api)
	{
		bIsLoading = false;
		OnMeshLoaded.Broadcast(false, TEXT("ApiClient not available"));
		return;
	}

	FString Endpoint = FString::Printf(
		TEXT("/api/mesh/json/%s/%s?lod=%s&max_triangles=%d"),
		*ProjectId, *ComponentName, *LODLevel, MaxJsonTriangles);

	FOnApiResponse Callback;
	TWeakObjectPtr<UHJMeshLoader> WeakThis(this);
	Callback.BindLambda([WeakThis](bool bSuccess, const FString& Response)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, bSuccess, Response]()
		{
			UHJMeshLoader* Self = WeakThis.Get();
			if (!Self) return;

			if (!bSuccess)
			{
				Self->bIsLoading = false;
				Self->OnMeshLoaded.Broadcast(false, TEXT("JSON mesh fetch failed"));
				return;
			}
			Self->ApplyMeshFromJson(Response);
		});
	});

	Api->Get(Endpoint, Callback);
}

void UHJMeshLoader::FetchMeshBinary(const FString& ProjectId, const FString& ComponentName)
{
	FString Url = FString::Printf(
		TEXT("http://127.0.0.1:5000/api/mesh/binary/%s/%s?lod=%s"),
		*ProjectId, *ComponentName, *LODLevel);

	FHttpModule& Http = FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http.CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/octet-stream"));
	Request->SetTimeout(60.0f);

	TWeakObjectPtr<UHJMeshLoader> WeakThis(this);
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnected)
		{
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Resp, bConnected]()
			{
				UHJMeshLoader* Self = WeakThis.Get();
				if (!Self) return;

				if (!bConnected || !Resp.IsValid() || Resp->GetResponseCode() != 200)
				{
					// Fallback to JSON mode
					UE_LOG(LogTemp, Warning,
						TEXT("HJMeshLoader: Binary fetch failed, falling back to JSON"));
					Self->FetchMeshJson(Self->CurrentProjectId, Self->CurrentComponentName);
					return;
				}

				Self->ApplyMeshFromBinary(Resp->GetContent());
			});
		});

	Request->ProcessRequest();
}

void UHJMeshLoader::ApplyMeshFromJson(const FString& JsonResponse)
{
	TSharedPtr<FJsonObject> Obj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonResponse);
	if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
	{
		bIsLoading = false;
		OnMeshLoaded.Broadcast(false, TEXT("Failed to parse mesh JSON"));
		return;
	}

	bool bSuccess = false;
	Obj->TryGetBoolField(TEXT("success"), bSuccess);
	if (!bSuccess)
	{
		FString Error;
		Obj->TryGetStringField(TEXT("error"), Error);
		bIsLoading = false;
		OnMeshLoaded.Broadcast(false, FString::Printf(TEXT("Server error: %s"), *Error));
		return;
	}

	// Parse flat arrays
	const TArray<TSharedPtr<FJsonValue>>* VertsArr = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* NormArr = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* IdxArr = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* UVArr = nullptr;

	if (!Obj->TryGetArrayField(TEXT("vertices"), VertsArr) ||
		!Obj->TryGetArrayField(TEXT("normals"), NormArr) ||
		!Obj->TryGetArrayField(TEXT("indices"), IdxArr))
	{
		bIsLoading = false;
		OnMeshLoaded.Broadcast(false, TEXT("Missing vertex/normal/index arrays"));
		return;
	}

	Obj->TryGetArrayField(TEXT("uvs"), UVArr);

	int32 VertCount = VertsArr->Num() / 3;
	int32 TriCount = IdxArr->Num() / 3;

	// Build vertex array
	TArray<FVector> Vertices;
	Vertices.SetNum(VertCount);
	for (int32 i = 0; i < VertCount; i++)
	{
		// CadQuery uses mm, UE5 uses cm — convert mm to cm
		float X = (*VertsArr)[i * 3 + 0]->AsNumber() * 0.1f;
		float Y = (*VertsArr)[i * 3 + 1]->AsNumber() * 0.1f;
		float Z = (*VertsArr)[i * 3 + 2]->AsNumber() * 0.1f;
		// CadQuery Y-up to UE5 Z-up: swap Y and Z
		Vertices[i] = FVector(X, Z, Y);
	}

	// Build normal array
	TArray<FVector> Normals;
	Normals.SetNum(VertCount);
	for (int32 i = 0; i < VertCount; i++)
	{
		float NX = (*NormArr)[i * 3 + 0]->AsNumber();
		float NY = (*NormArr)[i * 3 + 1]->AsNumber();
		float NZ = (*NormArr)[i * 3 + 2]->AsNumber();
		Normals[i] = FVector(NX, NZ, NY);
	}

	// Build index array
	TArray<int32> Triangles;
	Triangles.SetNum(IdxArr->Num());
	for (int32 i = 0; i < IdxArr->Num(); i++)
	{
		Triangles[i] = static_cast<int32>((*IdxArr)[i]->AsNumber());
	}

	// Build UV array
	TArray<FVector2D> UVs;
	if (UVArr && UVArr->Num() >= VertCount * 2)
	{
		UVs.SetNum(VertCount);
		for (int32 i = 0; i < VertCount; i++)
		{
			UVs[i] = FVector2D(
				(*UVArr)[i * 2 + 0]->AsNumber(),
				(*UVArr)[i * 2 + 1]->AsNumber()
			);
		}
	}

	CreateProceduralMesh(Vertices, Triangles, Normals, UVs);

	LoadedTriangleCount = TriCount;
	LoadedVertexCount = VertCount;
	bIsLoading = false;
	OnMeshLoaded.Broadcast(true, FString::Printf(
		TEXT("Loaded %d triangles, %d vertices"), TriCount, VertCount));
}

void UHJMeshLoader::ApplyMeshFromBinary(const TArray<uint8>& BinaryData)
{
	// Binary format header: 4x uint32 LE
	//   [0] vertex_count
	//   [1] triangle_count
	//   [2] has_normals (1=yes)
	//   [3] has_uvs (1=yes)

	if (BinaryData.Num() < 16)
	{
		bIsLoading = false;
		OnMeshLoaded.Broadcast(false, TEXT("Binary data too small"));
		return;
	}

	const uint8* Ptr = BinaryData.GetData();
	uint32 VertCount = *reinterpret_cast<const uint32*>(Ptr + 0);
	uint32 TriCount = *reinterpret_cast<const uint32*>(Ptr + 4);
	uint32 HasNormals = *reinterpret_cast<const uint32*>(Ptr + 8);
	uint32 HasUVs = *reinterpret_cast<const uint32*>(Ptr + 12);

	int32 Offset = 16;
	int32 VertBytes = VertCount * 3 * sizeof(float);
	int32 NormBytes = HasNormals ? VertCount * 3 * sizeof(float) : 0;
	int32 IdxBytes = TriCount * 3 * sizeof(uint32);
	int32 UVBytes = HasUVs ? VertCount * 2 * sizeof(float) : 0;

	int32 ExpectedSize = 16 + VertBytes + NormBytes + IdxBytes + UVBytes;
	if (BinaryData.Num() < ExpectedSize)
	{
		bIsLoading = false;
		OnMeshLoaded.Broadcast(false, FString::Printf(
			TEXT("Binary data too small: got %d, expected %d"),
			BinaryData.Num(), ExpectedSize));
		return;
	}

	// Parse vertices (mm to cm, Y-up to Z-up)
	const float* VertPtr = reinterpret_cast<const float*>(Ptr + Offset);
	TArray<FVector> Vertices;
	Vertices.SetNum(VertCount);
	for (uint32 i = 0; i < VertCount; i++)
	{
		float X = VertPtr[i * 3 + 0] * 0.1f;
		float Y = VertPtr[i * 3 + 1] * 0.1f;
		float Z = VertPtr[i * 3 + 2] * 0.1f;
		Vertices[i] = FVector(X, Z, Y);
	}
	Offset += VertBytes;

	// Parse normals
	TArray<FVector> Normals;
	if (HasNormals)
	{
		const float* NormPtr = reinterpret_cast<const float*>(Ptr + Offset);
		Normals.SetNum(VertCount);
		for (uint32 i = 0; i < VertCount; i++)
		{
			float NX = NormPtr[i * 3 + 0];
			float NY = NormPtr[i * 3 + 1];
			float NZ = NormPtr[i * 3 + 2];
			Normals[i] = FVector(NX, NZ, NY);
		}
		Offset += NormBytes;
	}

	// Parse indices
	const uint32* IdxPtr = reinterpret_cast<const uint32*>(Ptr + Offset);
	TArray<int32> Triangles;
	Triangles.SetNum(TriCount * 3);
	for (uint32 i = 0; i < TriCount * 3; i++)
	{
		Triangles[i] = static_cast<int32>(IdxPtr[i]);
	}
	Offset += IdxBytes;

	// Parse UVs
	TArray<FVector2D> UVs;
	if (HasUVs)
	{
		const float* UVPtr = reinterpret_cast<const float*>(Ptr + Offset);
		UVs.SetNum(VertCount);
		for (uint32 i = 0; i < VertCount; i++)
		{
			UVs[i] = FVector2D(UVPtr[i * 2 + 0], UVPtr[i * 2 + 1]);
		}
	}

	CreateProceduralMesh(Vertices, Triangles, Normals, UVs);

	LoadedTriangleCount = TriCount;
	LoadedVertexCount = VertCount;
	bIsLoading = false;
	OnMeshLoaded.Broadcast(true, FString::Printf(
		TEXT("Loaded %d triangles, %d vertices (binary)"), TriCount, VertCount));
}

void UHJMeshLoader::CreateProceduralMesh(const TArray<FVector>& Vertices,
                                          const TArray<int32>& Triangles,
                                          const TArray<FVector>& Normals,
                                          const TArray<FVector2D>& UVs)
{
	UProceduralMeshComponent* MeshComp = GetOrCreateMeshComponent();
	if (!MeshComp)
	{
		OnMeshLoaded.Broadcast(false, TEXT("No ProceduralMeshComponent available"));
		return;
	}

	// Clear previous mesh
	MeshComp->ClearAllMeshSections();

	// Empty tangent and vertex color arrays (UE5 will compute tangents)
	TArray<FLinearColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;

	/*
	 * ASYNC COLLISION COOKING:
	 * Step 1: Create mesh section with collision DISABLED so geometry renders immediately.
	 * Step 2: Cook collision on a background thread (avoids main-thread freeze).
	 * Step 3: Enable collision on the game thread once cooking completes.
	 *
	 * For meshes < 10k triangles, cook inline (fast enough, no hitch).
	 */
	bool bCookInline = !bEnableCollision || (Triangles.Num() / 3 < 10000);

	MeshComp->CreateMeshSection_LinearColor(
		0,           // Section index
		Vertices,
		Triangles,
		Normals,
		UVs,
		VertexColors,
		Tangents,
		bCookInline && bEnableCollision  // Only enable collision inline for small meshes
	);

	// Apply material if specified
	if (MeshMaterial)
	{
		MeshComp->SetMaterial(0, MeshMaterial);
	}

	int32 TriCount = Triangles.Num() / 3;
	UE_LOG(LogTemp, Log,
		TEXT("HJMeshLoader: Created procedural mesh — %d verts, %d tris%s"),
		Vertices.Num(), TriCount,
		(!bCookInline && bEnableCollision) ? TEXT(" (collision cooking async...)") : TEXT(""));

	// Step 2: Async collision cooking for large meshes
	if (!bCookInline && bEnableCollision)
	{
		CookCollisionAsync(MeshComp);
	}
}

void UHJMeshLoader::CookCollisionAsync(UProceduralMeshComponent* MeshComp)
{
	if (!MeshComp || bCookingCollision) return;

	bCookingCollision = true;

	/*
	 * Strategy: We need to rebuild the mesh section with collision enabled.
	 * ProceduralMeshComponent doesn't expose a direct "cook collision only" method,
	 * so we extract the existing section data, rebuild with collision on a deferred
	 * game-thread call after a short delay to let the first frame render.
	 *
	 * For UE5's ProceduralMeshComponent, the collision cooking happens inside
	 * CreateMeshSection when bCreateCollision=true. We use a timer to defer this
	 * to the next frame so the mesh is visible immediately.
	 */

	TWeakObjectPtr<UHJMeshLoader> WeakThis(this);
	TWeakObjectPtr<UProceduralMeshComponent> WeakMesh(MeshComp);

	// Defer collision cooking to next frame
	if (UWorld* World = GetWorld())
	{
		FTimerHandle TimerHandle;
		World->GetTimerManager().SetTimer(TimerHandle,
			FTimerDelegate::CreateLambda([WeakThis, WeakMesh]()
			{
				UHJMeshLoader* Self = WeakThis.Get();
				UProceduralMeshComponent* Mesh = WeakMesh.Get();
				if (!Self || !Mesh) return;

				// Get section data from section 0
				FProcMeshSection* Section = Mesh->GetProcMeshSection(0);
				if (!Section || Section->ProcVertexBuffer.Num() == 0)
				{
					Self->bCookingCollision = false;
					return;
				}

				// Extract arrays from existing section
				TArray<FVector> Vertices;
				TArray<FVector> Normals;
				TArray<FVector2D> UVs;
				TArray<FLinearColor> Colors;
				TArray<int32> Triangles;
				TArray<FProcMeshTangent> Tangents;

				Vertices.SetNum(Section->ProcVertexBuffer.Num());
				Normals.SetNum(Section->ProcVertexBuffer.Num());
				UVs.SetNum(Section->ProcVertexBuffer.Num());
				Tangents.SetNum(Section->ProcVertexBuffer.Num());

				for (int32 i = 0; i < Section->ProcVertexBuffer.Num(); i++)
				{
					const FProcMeshVertex& V = Section->ProcVertexBuffer[i];
					Vertices[i] = V.Position;
					Normals[i] = V.Normal;
					UVs[i] = V.UV0;
					Tangents[i] = V.Tangent;
				}

				Triangles = Section->ProcIndexBuffer;

				// Rebuild with collision enabled — this cooks physics
				Mesh->CreateMeshSection_LinearColor(
					0, Vertices, Triangles, Normals, UVs, Colors, Tangents, true);

				// Re-apply material
				if (Self->MeshMaterial)
				{
					Mesh->SetMaterial(0, Self->MeshMaterial);
				}

				Self->bCookingCollision = false;
				UE_LOG(LogTemp, Log,
					TEXT("HJMeshLoader: Async collision cooking complete (%d tris)"),
					Triangles.Num() / 3);
			}),
			0.1f,  // 100ms delay — 6 frames at 60fps, enough to render first
			false);
	}
}

// ── Static Utility ─────────────────────────────────────────────

bool UHJMeshLoader::LoadMeshFromJsonString(UProceduralMeshComponent* TargetComp, const FString& JsonString)
{
	if (!TargetComp) return false;

	TSharedPtr<FJsonObject> Obj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("HJMeshLoader::LoadMeshFromJsonString: Failed to parse JSON"));
		return false;
	}

	bool bSuccess = false;
	Obj->TryGetBoolField(TEXT("success"), bSuccess);
	if (!bSuccess)
	{
		FString Error;
		Obj->TryGetStringField(TEXT("error"), Error);
		UE_LOG(LogTemp, Warning, TEXT("HJMeshLoader::LoadMeshFromJsonString: Server error: %s"), *Error);
		return false;
	}

	// Parse flat arrays
	const TArray<TSharedPtr<FJsonValue>>* VertsArr = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* NormArr = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* IdxArr = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* UVArr = nullptr;

	if (!Obj->TryGetArrayField(TEXT("vertices"), VertsArr) ||
		!Obj->TryGetArrayField(TEXT("normals"), NormArr) ||
		!Obj->TryGetArrayField(TEXT("indices"), IdxArr))
	{
		UE_LOG(LogTemp, Warning, TEXT("HJMeshLoader::LoadMeshFromJsonString: Missing vertex/normal/index arrays"));
		return false;
	}

	Obj->TryGetArrayField(TEXT("uvs"), UVArr);

	int32 VertCount = VertsArr->Num() / 3;
	int32 TriCount = IdxArr->Num() / 3;

	// Build vertex array (mm to cm, Y-up to Z-up)
	TArray<FVector> Vertices;
	Vertices.SetNum(VertCount);
	for (int32 i = 0; i < VertCount; i++)
	{
		float X = (*VertsArr)[i * 3 + 0]->AsNumber() * 0.1f;
		float Y = (*VertsArr)[i * 3 + 1]->AsNumber() * 0.1f;
		float Z = (*VertsArr)[i * 3 + 2]->AsNumber() * 0.1f;
		Vertices[i] = FVector(X, Z, Y);
	}

	// Build normal array
	TArray<FVector> Normals;
	Normals.SetNum(VertCount);
	for (int32 i = 0; i < VertCount; i++)
	{
		float NX = (*NormArr)[i * 3 + 0]->AsNumber();
		float NY = (*NormArr)[i * 3 + 1]->AsNumber();
		float NZ = (*NormArr)[i * 3 + 2]->AsNumber();
		Normals[i] = FVector(NX, NZ, NY);
	}

	// Build index array
	TArray<int32> Triangles;
	Triangles.SetNum(IdxArr->Num());
	for (int32 i = 0; i < IdxArr->Num(); i++)
	{
		Triangles[i] = static_cast<int32>((*IdxArr)[i]->AsNumber());
	}

	// Build UV array
	TArray<FVector2D> UVs;
	if (UVArr && UVArr->Num() >= VertCount * 2)
	{
		UVs.SetNum(VertCount);
		for (int32 i = 0; i < VertCount; i++)
		{
			UVs[i] = FVector2D(
				(*UVArr)[i * 2 + 0]->AsNumber(),
				(*UVArr)[i * 2 + 1]->AsNumber()
			);
		}
	}

	// Clear and rebuild mesh
	TargetComp->ClearAllMeshSections();

	TArray<FLinearColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;

	TargetComp->CreateMeshSection_LinearColor(
		0, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, false);

	UE_LOG(LogTemp, Log,
		TEXT("HJMeshLoader::LoadMeshFromJsonString: Created weapon mesh — %d verts, %d tris"),
		VertCount, TriCount);

	return true;
}

// ── Material Presets ───────────────────────────────────────────

void UHJMeshLoader::ApplyMaterialPreset(UProceduralMeshComponent* TargetComp, const FString& PresetName)
{
	if (!TargetComp) return;

	UMaterialInstanceDynamic* DynMat = TargetComp->CreateAndSetMaterialInstanceDynamic(0);
	if (!DynMat) return;

	FString Key = PresetName.ToLower();

	if (Key == TEXT("bronze"))
	{
		DynMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.72f, 0.45f, 0.20f));
		DynMat->SetScalarParameterValue(TEXT("Metallic"), 0.85f);
		DynMat->SetScalarParameterValue(TEXT("Roughness"), 0.35f);
	}
	else if (Key == TEXT("void_crystal"))
	{
		DynMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.15f, 0.05f, 0.35f));
		DynMat->SetScalarParameterValue(TEXT("Metallic"), 0.2f);
		DynMat->SetScalarParameterValue(TEXT("Roughness"), 0.1f);
		DynMat->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(0.4f, 0.1f, 0.8f) * 3.f);
	}
	else if (Key == TEXT("aether_glow"))
	{
		DynMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.90f, 0.78f, 0.30f));
		DynMat->SetScalarParameterValue(TEXT("Metallic"), 0.95f);
		DynMat->SetScalarParameterValue(TEXT("Roughness"), 0.15f);
		DynMat->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(1.0f, 0.85f, 0.3f) * 5.f);
	}
	else if (Key == TEXT("worn_stone"))
	{
		DynMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.45f, 0.40f, 0.35f));
		DynMat->SetScalarParameterValue(TEXT("Metallic"), 0.0f);
		DynMat->SetScalarParameterValue(TEXT("Roughness"), 0.85f);
	}
	else if (Key == TEXT("dark_iron"))
	{
		DynMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.18f, 0.18f, 0.22f));
		DynMat->SetScalarParameterValue(TEXT("Metallic"), 0.90f);
		DynMat->SetScalarParameterValue(TEXT("Roughness"), 0.4f);
		DynMat->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(0.3f, 0.1f, 0.0f) * 1.5f);
	}
	else
	{
		// Default neutral
		DynMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.5f, 0.5f, 0.5f));
		DynMat->SetScalarParameterValue(TEXT("Metallic"), 0.3f);
		DynMat->SetScalarParameterValue(TEXT("Roughness"), 0.5f);
	}
}

void UHJMeshLoader::ApplyRarityMaterial(UProceduralMeshComponent* TargetComp, const FString& Rarity)
{
	FString Key = Rarity.ToUpper();
	if (Key == TEXT("LEGENDARY"))      ApplyMaterialPreset(TargetComp, TEXT("aether_glow"));
	else if (Key == TEXT("EPIC"))      ApplyMaterialPreset(TargetComp, TEXT("void_crystal"));
	else if (Key == TEXT("RARE"))      ApplyMaterialPreset(TargetComp, TEXT("dark_iron"));
	else if (Key == TEXT("UNCOMMON"))  ApplyMaterialPreset(TargetComp, TEXT("bronze"));
	else                                ApplyMaterialPreset(TargetComp, TEXT("worn_stone"));
}

void UHJMeshLoader::PlayMeshReveal(AActor* MeshActor)
{
	if (!MeshActor) return;

	// Store original scale and start at zero
	FVector OriginalScale = MeshActor->GetActorScale3D();
	MeshActor->SetActorScale3D(FVector::ZeroVector);

	// Create a timeline-like reveal using timers
	// Phase 1: Scale up from 0 to 1.1x over 0.35s (overshoot)
	// Phase 2: Scale down from 1.1x to 1.0x over 0.15s (settle)
	// Total: 0.5s reveal

	TWeakObjectPtr<AActor> WeakActor(MeshActor);
	FVector TargetScale = OriginalScale;

	// Apply emissive flash to all ProceduralMeshComponents
	TArray<UProceduralMeshComponent*> MeshComps;
	MeshActor->GetComponents<UProceduralMeshComponent>(MeshComps);
	for (UProceduralMeshComponent* MC : MeshComps)
	{
		if (MC)
		{
			UMaterialInstanceDynamic* DynMat = MC->CreateAndSetMaterialInstanceDynamic(0);
			if (DynMat)
			{
				DynMat->SetVectorParameterValue(TEXT("EmissiveColor"),
					FLinearColor(1.0f, 0.85f, 0.3f) * 8.f);  // Bright golden flash
			}
		}
	}

	// Phase 1 timer: scale up over 7 frames (~0.35s at 20 fps tick)
	UWorld* World = MeshActor->GetWorld();
	if (!World) return;

	FTimerHandle RevealTimer;
	float Duration = 0.35f;
	float OvershootScale = 1.1f;

	// Use a latent action: scale from 0 to 1.1x
	World->GetTimerManager().SetTimer(RevealTimer, [WeakActor, TargetScale, OvershootScale]()
	{
		AActor* A = WeakActor.Get();
		if (!A) return;

		FVector CurrentScale = A->GetActorScale3D();
		FVector Target = TargetScale * OvershootScale;
		FVector NewScale = FMath::VInterpTo(CurrentScale, Target, 0.033f, 8.f);
		A->SetActorScale3D(NewScale);
	}, 0.033f, true);

	// Phase 2: after 0.35s, settle to 1.0x and fade emissive
	FTimerHandle SettleTimer;
	World->GetTimerManager().SetTimer(SettleTimer, [WeakActor, TargetScale, RevealTimer, World]() mutable
	{
		AActor* A = WeakActor.Get();
		if (!A) return;

		// Cancel overshoot timer
		if (World)
			World->GetTimerManager().ClearTimer(RevealTimer);

		// Set exact target scale
		A->SetActorScale3D(TargetScale);

		// Fade emissive back to normal
		TArray<UProceduralMeshComponent*> MeshComps;
		A->GetComponents<UProceduralMeshComponent>(MeshComps);
		for (UProceduralMeshComponent* MC : MeshComps)
		{
			if (MC)
			{
				UMaterialInstanceDynamic* DynMat = Cast<UMaterialInstanceDynamic>(MC->GetMaterial(0));
				if (DynMat)
				{
					DynMat->SetVectorParameterValue(TEXT("EmissiveColor"),
						FLinearColor::Black);
				}
			}
		}
	}, Duration + 0.15f, false);
}
