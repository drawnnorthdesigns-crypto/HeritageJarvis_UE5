#include "TartariaResourceNode.h"
#include "Core/HJGameInstance.h"
#include "Core/HJApiClient.h"
#include "UI/HJNotificationWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
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
