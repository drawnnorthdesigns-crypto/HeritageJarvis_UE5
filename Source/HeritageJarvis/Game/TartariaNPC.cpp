#include "TartariaNPC.h"
#include "Core/HJGameInstance.h"
#include "Core/HJApiClient.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Async/Async.h"

ATartariaNPC::ATartariaNPC()
{
	PrimaryActorTick.bCanEverTick = false;

	// Cylinder body mesh attached to capsule for visibility
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(
		TEXT("/Engine/BasicShapes/Cylinder"));
	if (MeshFinder.Succeeded())
		BodyMesh->SetStaticMesh(MeshFinder.Object);
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, -45.f));
	BodyMesh->SetWorldScale3D(FVector(0.5f, 0.5f, 1.0f));
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ATartariaNPC::OnInteract_Implementation(APlayerController* Interactor)
{
	UE_LOG(LogTemp, Log, TEXT("TartariaNPC: Player interacted with %s (%s)"),
		*NPCName, *FactionKey);

	// Send initial greeting dialogue
	SendDialogueRequest(Interactor, TEXT("Greetings."));

	// Fire Blueprint event for custom UI
	OnDialogueStarted(Interactor, TEXT("{}"));
}

FString ATartariaNPC::GetInteractPrompt_Implementation() const
{
	return FString::Printf(TEXT("Speak with %s"), *NPCName);
}

void ATartariaNPC::SendDialogueRequest(APlayerController* Interactor, const FString& PlayerMessage)
{
	UHJGameInstance* GI = UHJGameInstance::Get(this);
	if (!GI || !GI->ApiClient)
	{
		OnDialogueError();
		return;
	}

	// Build JSON body
	TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject());
	Body->SetStringField(TEXT("npc_name"), NPCName);
	Body->SetStringField(TEXT("faction"), FactionKey);
	Body->SetStringField(TEXT("persona"), PersonaPrompt);
	Body->SetStringField(TEXT("message"), PlayerMessage);

	FString BodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	TWeakObjectPtr<ATartariaNPC> WeakThis(this);

	FOnApiResponse CB;
	CB.BindLambda([WeakThis](bool bOk, const FString& RespBody)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, bOk, RespBody]()
		{
			ATartariaNPC* Self = WeakThis.Get();
			if (!Self) return;

			if (!bOk)
			{
				Self->OnDialogueError();
				return;
			}

			// Parse response
			TSharedPtr<FJsonObject> Json;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RespBody);
			if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
			{
				FString Response;
				if (Json->TryGetStringField(TEXT("response"), Response))
				{
					Self->OnDialogueResponse(Response);
				}
				else
				{
					Self->OnDialogueError();
				}
			}
			else
			{
				Self->OnDialogueError();
			}
		});
	});

	GI->ApiClient->Post(DialogueEndpoint, BodyStr, CB);
}
