#include "TartariaQuestMarker.h"
#include "Core/HJGameInstance.h"
#include "Core/HJApiClient.h"
#include "UI/HJNotificationWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/CapsuleComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

ATartariaQuestMarker::ATartariaQuestMarker()
{
	PrimaryActorTick.bCanEverTick = true;

	// Interaction capsule as root (enables line-trace hit)
	InteractCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("InteractCapsule"));
	InteractCapsule->SetCapsuleHalfHeight(80.f);
	InteractCapsule->SetCapsuleRadius(50.f);
	InteractCapsule->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	InteractCapsule->SetGenerateOverlapEvents(true);
	RootComponent = InteractCapsule;

	// Marker mesh (assign diamond/crystal shape in editor)
	MarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerMesh"));
	MarkerMesh->SetupAttachment(RootComponent);
	MarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Glowing light indicator
	MarkerLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MarkerLight"));
	MarkerLight->SetupAttachment(RootComponent);
	MarkerLight->SetRelativeLocation(FVector(0.f, 0.f, 20.f));
	MarkerLight->SetIntensity(5000.f);
	MarkerLight->SetLightColor(FLinearColor(1.0f, 0.9f, 0.3f)); // Gold glow
	MarkerLight->SetAttenuationRadius(500.f);
}

void ATartariaQuestMarker::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bActive) return;

	// Bob animation
	BobTimer += DeltaTime * BobSpeed;
	float BobOffset = FMath::Sin(BobTimer * 2.f * PI) * BobAmplitude;

	if (InitialLocation.IsZero())
		InitialLocation = GetActorLocation();

	FVector NewLocation = InitialLocation;
	NewLocation.Z += BobOffset;
	SetActorLocation(NewLocation);

	// Slow rotation
	FRotator CurrentRotation = GetActorRotation();
	CurrentRotation.Yaw += 45.f * DeltaTime; // 45 degrees per second
	SetActorRotation(CurrentRotation);
}

// -------------------------------------------------------
// IHJInteractable
// -------------------------------------------------------

void ATartariaQuestMarker::OnInteract_Implementation(APlayerController* Interactor)
{
	if (!bActive) return;

	UE_LOG(LogTemp, Log, TEXT("TartariaQuestMarker: Player interacted with quest '%s' (step %d)"),
		*QuestTitle, CurrentStep);

	// Advance quest step
	CurrentStep++;

	// Notify delegates
	OnQuestInteractedDelegate.Broadcast(QuestId, CurrentStep);
	OnQuestAdvanced(CurrentStep);

	// Send advance request to Flask
	SendAdvanceRequest();

	// Show toast feedback based on quest type
	FString Msg;
	EHJNotifType NotifType = EHJNotifType::Info;

	if (QuestType == TEXT("exploration"))
	{
		Msg = FString::Printf(TEXT("Quest: %s (Step %d)"), *QuestTitle, CurrentStep);
		NotifType = EHJNotifType::Info;
	}
	else if (QuestType == TEXT("faction"))
	{
		Msg = FString::Printf(TEXT("Faction Quest: %s"), *QuestTitle);
		NotifType = EHJNotifType::Success;
	}
	else if (QuestType == TEXT("threat"))
	{
		Msg = FString::Printf(TEXT("Threat Quest: %s — Prepare for battle!"), *QuestTitle);
		NotifType = EHJNotifType::Warning;
	}
	else if (QuestType == TEXT("event"))
	{
		Msg = FString::Printf(TEXT("Event: %s"), *QuestTitle);
		NotifType = EHJNotifType::Info;
	}
	else
	{
		Msg = FString::Printf(TEXT("%s (Step %d)"), *QuestTitle, CurrentStep);
	}

	UHJNotificationWidget::Toast(Msg, NotifType, 3.0f);
}

FString ATartariaQuestMarker::GetInteractPrompt_Implementation() const
{
	if (!bActive)
		return FString::Printf(TEXT("[Completed] %s"), *QuestTitle);

	if (CurrentStep == 0)
		return FString::Printf(TEXT("Begin: %s"), *QuestTitle);

	return FString::Printf(TEXT("Continue: %s (Step %d)"), *QuestTitle, CurrentStep);
}

// -------------------------------------------------------
// Network
// -------------------------------------------------------

void ATartariaQuestMarker::SendAdvanceRequest()
{
	UHJGameInstance* GI = UHJGameInstance::Get(this);
	if (!GI || !GI->ApiClient) return;

	TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject());
	Body->SetNumberField(TEXT("step"), CurrentStep);
	if (CurrentStep >= 3)  // Auto-complete after 3 steps
		Body->SetStringField(TEXT("status"), TEXT("completed"));

	FString BodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	FString Endpoint = FString::Printf(TEXT("/api/game/quests/%s/advance"), *QuestId);

	FOnApiResponse CB;
	CB.BindUObject(this, &ATartariaQuestMarker::OnAdvanceResponse);
	GI->ApiClient->Post(Endpoint, BodyStr, CB);
}

void ATartariaQuestMarker::OnAdvanceResponse(bool bSuccess, const FString& Body)
{
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("TartariaQuestMarker: Failed to advance quest '%s'"), *QuestId);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("TartariaQuestMarker: Quest '%s' advanced to step %d"), *QuestId, CurrentStep);

	// Auto-complete after 3 steps
	if (CurrentStep >= 3)
	{
		CompleteQuest();
		UHJNotificationWidget::Toast(
			FString::Printf(TEXT("Quest Complete: %s"), *QuestTitle),
			EHJNotifType::Success, 4.0f);
	}
}

// -------------------------------------------------------
// Quest state
// -------------------------------------------------------

void ATartariaQuestMarker::ActivateQuest()
{
	bActive = true;
	CurrentStep = 0;
	SetActorHiddenInGame(false);
	if (MarkerLight) MarkerLight->SetVisibility(true);
	OnQuestActivated();

	UE_LOG(LogTemp, Log, TEXT("TartariaQuestMarker: Quest '%s' activated"), *QuestTitle);
}

void ATartariaQuestMarker::CompleteQuest()
{
	bActive = false;
	SetActorHiddenInGame(true);
	if (MarkerLight) MarkerLight->SetVisibility(false);
	OnQuestCompleted();

	UE_LOG(LogTemp, Log, TEXT("TartariaQuestMarker: Quest '%s' completed"), *QuestTitle);
}
