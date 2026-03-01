#include "TartariaResourceNode.h"
#include "Core/HJGameInstance.h"
#include "Core/HJApiClient.h"
#include "Core/TartariaSoundManager.h"
#include "UI/HJNotificationWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Async/Async.h"

ATartariaResourceNode::ATartariaResourceNode()
{
	PrimaryActorTick.bCanEverTick = true;

	// Resource mesh — sphere primitive for visibility
	ResourceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ResourceMesh"));
	RootComponent = ResourceMesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(
		TEXT("/Engine/BasicShapes/Sphere"));
	if (MeshFinder.Succeeded())
		ResourceMesh->SetStaticMesh(MeshFinder.Object);
	ResourceMesh->SetWorldScale3D(FVector(0.75f));

	// Glow light to make resources visible
	GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLight"));
	GlowLight->SetupAttachment(RootComponent);
	GlowLight->SetRelativeLocation(FVector(0.f, 0.f, 50.f));
	GlowLight->SetIntensity(3000.f);
	GlowLight->SetLightColor(FLinearColor(0.5f, 0.8f, 1.0f)); // Blue-white glow
	GlowLight->SetAttenuationRadius(400.f);
}

void ATartariaResourceNode::BeginPlay()
{
	Super::BeginPlay();

	// ── Resource Type Visual Differentiation ─────────────────
	if (ResourceMesh)
	{
		// Different scale and color per resource type
		if (ResourceType == ETartariaResourceType::Iron)
		{
			ResourceMesh->SetRelativeScale3D(FVector(1.2f, 0.8f, 0.6f));  // angular, flat
			if (GlowLight) GlowLight->SetLightColor(FLinearColor(0.8f, 0.4f, 0.2f));  // orange-red
		}
		else if (ResourceType == ETartariaResourceType::Stone)
		{
			ResourceMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.7f));  // round, squat
			if (GlowLight) GlowLight->SetLightColor(FLinearColor(0.6f, 0.6f, 0.5f));  // warm gray
		}
		else if (ResourceType == ETartariaResourceType::Knowledge)
		{
			ResourceMesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.4f));  // tall, thin (book shape)
			if (GlowLight) GlowLight->SetLightColor(FLinearColor(0.3f, 0.5f, 1.0f));  // blue
		}
		else if (ResourceType == ETartariaResourceType::Crystal)
		{
			ResourceMesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 1.6f));  // tall spiky
			if (GlowLight) GlowLight->SetLightColor(FLinearColor(0.8f, 0.2f, 0.9f));  // purple
		}
	}
}

void ATartariaResourceNode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Handle respawn timer
	if (bDepleted)
	{
		RespawnTimer += DeltaTime;
		if (RespawnTimer >= RespawnTime)
		{
			Respawn();
		}
	}
}

void ATartariaResourceNode::OnInteract_Implementation(APlayerController* Interactor)
{
	if (bDepleted)
	{
		UE_LOG(LogTemp, Log, TEXT("TartariaResourceNode: Node is depleted"));
		return;
	}

	// Harvest one unit
	int32 Gathered = FMath::Min(1, RemainingYield);
	RemainingYield -= Gathered;

	UE_LOG(LogTemp, Log, TEXT("TartariaResourceNode: Harvested %d, remaining %d/%d"),
		Gathered, RemainingYield, MaxYield);

	UTartariaSoundManager::PlayHarvest(this);

	// Visual harvest feedback — floating orbs rise from node
	SpawnHarvestEffect();

	// Fire Blueprint event
	OnHarvested(Gathered, RemainingYield);

	// Send to backend
	SendHarvestRequest(Interactor);

	// Check depletion
	if (RemainingYield <= 0)
	{
		bDepleted = true;
		RespawnTimer = 0.f;
		if (GlowLight) GlowLight->SetVisibility(false);
		if (ResourceMesh) ResourceMesh->SetVisibility(false);
		OnDepleted();
	}
}

FString ATartariaResourceNode::GetInteractPrompt_Implementation() const
{
	if (bDepleted) return TEXT("Depleted");

	FString TypeName;
	switch (ResourceType)
	{
	case ETartariaResourceType::Iron:      TypeName = TEXT("Iron"); break;
	case ETartariaResourceType::Stone:     TypeName = TEXT("Stone"); break;
	case ETartariaResourceType::Knowledge: TypeName = TEXT("Knowledge"); break;
	case ETartariaResourceType::Crystal:   TypeName = TEXT("Crystal"); break;
	}
	return FString::Printf(TEXT("Harvest %s (%d remaining)"), *TypeName, RemainingYield);
}

void ATartariaResourceNode::SendHarvestRequest(APlayerController* Interactor)
{
	UHJGameInstance* GI = UHJGameInstance::Get(this);
	if (!GI || !GI->ApiClient) return;

	// Build JSON body
	TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject());

	FString TypeStr;
	switch (ResourceType)
	{
	case ETartariaResourceType::Iron:      TypeStr = TEXT("iron"); break;
	case ETartariaResourceType::Stone:     TypeStr = TEXT("stone"); break;
	case ETartariaResourceType::Knowledge: TypeStr = TEXT("knowledge"); break;
	case ETartariaResourceType::Crystal:   TypeStr = TEXT("crystal"); break;
	}
	Body->SetStringField(TEXT("resource_type"), TypeStr);
	Body->SetNumberField(TEXT("amount"), 1);

	FString BodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	FOnApiResponse CB;
	CB.BindLambda([](bool bOk, const FString& Resp)
	{
		if (bOk)
		{
			UE_LOG(LogTemp, Log, TEXT("TartariaResourceNode: Harvest recorded on backend"));

			// Parse credits earned from response
			TSharedPtr<FJsonObject> Json;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Resp);
			if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
			{
				double Earned = 0;
				Json->TryGetNumberField(TEXT("credits_earned"), Earned);
				if (Earned > 0)
				{
					FString Msg = FString::Printf(TEXT("+%.0f credits"), Earned);
					AsyncTask(ENamedThreads::GameThread, [Msg]()
					{
						UHJNotificationWidget::Toast(Msg, EHJNotifType::Success, 2.0f);
					});
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("TartariaResourceNode: Failed to record harvest"));
		}
	});

	GI->ApiClient->Post(TEXT("/api/game/harvest"), BodyStr, CB);
}

// -------------------------------------------------------
// Environmental Effects — Harvest Particles
// -------------------------------------------------------

void ATartariaResourceNode::SpawnHarvestEffect()
{
	UWorld* World = GetWorld();
	if (!World) return;

	FVector BaseLoc = GetActorLocation() + FVector(0.f, 0.f, 50.f);

	// Spawn 3-5 small orbs that float upward
	int32 OrbCount = FMath::RandRange(3, 5);
	for (int32 i = 0; i < OrbCount; ++i)
	{
		FVector Offset(
			FMath::RandRange(-30.f, 30.f),
			FMath::RandRange(-30.f, 30.f),
			FMath::RandRange(0.f, 20.f)
		);

		AActor* Orb = World->SpawnActor<AActor>(AActor::StaticClass(), BaseLoc + Offset);
		if (!Orb) continue;

		UStaticMeshComponent* OrbMesh = NewObject<UStaticMeshComponent>(Orb);
		OrbMesh->RegisterComponent();
		Orb->SetRootComponent(OrbMesh);

		UStaticMesh* SphereSM = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere"));
		if (SphereSM) OrbMesh->SetStaticMesh(SphereSM);

		OrbMesh->SetWorldScale3D(FVector(0.05f));
		OrbMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// Color based on resource type enum
		FLinearColor OrbColor(0.3f, 0.8f, 0.4f);  // Default green (organic)
		switch (ResourceType)
		{
		case ETartariaResourceType::Crystal:
			OrbColor = FLinearColor(0.5f, 0.3f, 0.9f);  // Purple for crystals
			break;
		case ETartariaResourceType::Iron:
			OrbColor = FLinearColor(0.6f, 0.55f, 0.5f);  // Grey for metals
			break;
		case ETartariaResourceType::Stone:
			OrbColor = FLinearColor(0.5f, 0.45f, 0.35f);  // Warm stone brown
			break;
		case ETartariaResourceType::Knowledge:
			OrbColor = FLinearColor(0.3f, 0.5f, 1.0f);  // Blue for knowledge
			break;
		}

		UMaterialInstanceDynamic* DynMat = OrbMesh->CreateAndSetMaterialInstanceDynamic(0);
		if (DynMat)
		{
			DynMat->SetVectorParameterValue(TEXT("Color"), OrbColor);
			DynMat->SetVectorParameterValue(TEXT("EmissiveColor"), OrbColor * 3.f);
		}

		// Add a small glow
		UPointLightComponent* Glow = NewObject<UPointLightComponent>(Orb);
		Glow->RegisterComponent();
		Glow->SetupAttachment(OrbMesh);
		Glow->SetIntensity(500.f);
		Glow->SetAttenuationRadius(50.f);
		Glow->SetLightColor(OrbColor);
		Glow->SetCastShadows(false);

		Orb->SetLifeSpan(1.5f);

		// Float upward using timer
		TWeakObjectPtr<AActor> WeakOrb(Orb);
		float Speed = FMath::RandRange(80.f, 150.f);
		FTimerHandle FloatTimer;
		World->GetTimerManager().SetTimer(FloatTimer, [WeakOrb, Speed]()
		{
			AActor* O = WeakOrb.Get();
			if (O)
			{
				FVector Loc = O->GetActorLocation();
				O->SetActorLocation(Loc + FVector(0.f, 0.f, Speed * 0.033f));
				// Shrink slightly as it rises
				FVector Scale = O->GetActorScale3D() * 0.97f;
				O->SetActorScale3D(Scale);
			}
		}, 0.033f, true);
	}
}

void ATartariaResourceNode::Respawn()
{
	bDepleted = false;
	RespawnTimer = 0.f;
	RemainingYield = MaxYield;

	if (GlowLight) GlowLight->SetVisibility(true);
	if (ResourceMesh) ResourceMesh->SetVisibility(true);

	UE_LOG(LogTemp, Log, TEXT("TartariaResourceNode: Respawned"));
	OnRespawned();
}
