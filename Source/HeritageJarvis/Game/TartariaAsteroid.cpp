#include "TartariaAsteroid.h"
#include "Core/HJGameInstance.h"
#include "Core/HJApiClient.h"
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

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

ATartariaAsteroid::ATartariaAsteroid()
{
	PrimaryActorTick.bCanEverTick = true;

	// Asteroid body — sphere from BasicShapes
	AsteroidMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AsteroidMesh"));
	RootComponent = AsteroidMesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
		TEXT("/Engine/BasicShapes/Sphere"));
	if (SphereFinder.Succeeded())
		AsteroidMesh->SetStaticMesh(SphereFinder.Object);

	// Random scale in [0.5, 3.0]; seeded deterministically per asteroid
	float ScaleX = FMath::RandRange(0.5f, 3.0f);
	float ScaleY = FMath::RandRange(0.5f, 3.0f);
	float ScaleZ = FMath::RandRange(0.5f, 3.0f);
	AsteroidMesh->SetWorldScale3D(FVector(ScaleX, ScaleY, ScaleZ));

	// Random initial rotation
	FRotator InitRot(
		FMath::RandRange(0.f, 360.f),
		FMath::RandRange(0.f, 360.f),
		FMath::RandRange(0.f, 360.f));
	AsteroidMesh->SetRelativeRotation(InitRot);

	AsteroidMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	// Resource glow light attached above the mesh centre
	ResourceGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("ResourceGlow"));
	ResourceGlow->SetupAttachment(RootComponent);
	ResourceGlow->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
	ResourceGlow->SetIntensity(4000.f);
	ResourceGlow->SetAttenuationRadius(500.f);
	ResourceGlow->SetCastShadows(false);

	// Default glow colour (overridden in BeginPlay based on AsteroidType)
	ResourceGlow->SetLightColor(FLinearColor(0.8f, 0.7f, 0.5f));  // warm yellow-white

	// Tumble rates — slow, natural drift
	TumbleRate = FVector(
		FMath::RandRange(1.f, 4.f),
		FMath::RandRange(0.5f, 3.f),
		FMath::RandRange(0.2f, 2.f));
}

// ---------------------------------------------------------------------------
// BeginPlay
// ---------------------------------------------------------------------------

void ATartariaAsteroid::BeginPlay()
{
	Super::BeginPlay();

	if (!AsteroidMesh) return;

	switch (AsteroidType)
	{
	case EAsteroidType::Metallic:
		// Iron-grey body, orange glow
		AsteroidMesh->SetRelativeScale3D(AsteroidMesh->GetRelativeScale3D() * FVector(1.0f, 1.2f, 0.9f));
		if (ResourceGlow) ResourceGlow->SetLightColor(FLinearColor(1.0f, 0.55f, 0.1f));
		break;

	case EAsteroidType::Icy:
		// White-blue shimmer, cyan glow
		AsteroidMesh->SetRelativeScale3D(AsteroidMesh->GetRelativeScale3D() * FVector(1.1f, 1.1f, 1.3f));
		if (ResourceGlow) ResourceGlow->SetLightColor(FLinearColor(0.3f, 0.8f, 1.0f));
		break;

	case EAsteroidType::Rocky:
	default:
		// Brown irregular body, warm yellow glow
		AsteroidMesh->SetRelativeScale3D(AsteroidMesh->GetRelativeScale3D() * FVector(0.9f, 1.0f, 0.8f));
		if (ResourceGlow) ResourceGlow->SetLightColor(FLinearColor(0.9f, 0.75f, 0.3f));
		break;
	}

	UpdateVisuals();
}

// ---------------------------------------------------------------------------
// Tick — slow tumble
// ---------------------------------------------------------------------------

void ATartariaAsteroid::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (AsteroidMesh && CurrentYield > 0.f)
	{
		FRotator Delta(
			TumbleRate.X * DeltaTime,
			TumbleRate.Y * DeltaTime,
			TumbleRate.Z * DeltaTime);
		AsteroidMesh->AddRelativeRotation(Delta);
	}
}

// ---------------------------------------------------------------------------
// IHJInteractable
// ---------------------------------------------------------------------------

void ATartariaAsteroid::OnInteract_Implementation(APlayerController* Interactor)
{
	if (CurrentYield <= 0.f)
	{
		UE_LOG(LogTemp, Log, TEXT("TartariaAsteroid [%s]: Fully depleted"), *AsteroidId);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("TartariaAsteroid [%s]: Starting extraction (%.0f units)"),
		*AsteroidId, ExtractAmount);

	// Show "Mining..." floating notification
	UHJNotificationWidget::Toast(
		FString::Printf(TEXT("Mining %s..."), *AsteroidId),
		EHJNotifType::Info, 2.0f);

	SendMineRequest();
}

FString ATartariaAsteroid::GetInteractPrompt_Implementation() const
{
	if (CurrentYield <= 0.f)
		return TEXT("Asteroid depleted");

	FString TypeStr;
	switch (AsteroidType)
	{
	case EAsteroidType::Metallic: TypeStr = TEXT("Metallic Asteroid"); break;
	case EAsteroidType::Icy:      TypeStr = TEXT("Icy Asteroid");      break;
	case EAsteroidType::Rocky:    TypeStr = TEXT("Rocky Asteroid");    break;
	default:                      TypeStr = TEXT("Asteroid");          break;
	}

	return FString::Printf(TEXT("Mine %s [%.0f / %.0f]"),
		*TypeStr, CurrentYield, MaxYield);
}

// ---------------------------------------------------------------------------
// Backend call
// ---------------------------------------------------------------------------

void ATartariaAsteroid::SendMineRequest()
{
	UHJGameInstance* GI = UHJGameInstance::Get(this);
	if (!GI || !GI->ApiClient)
	{
		UE_LOG(LogTemp, Warning, TEXT("TartariaAsteroid: No GameInstance or ApiClient"));
		return;
	}

	TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject());
	Body->SetStringField(TEXT("asteroid_id"), AsteroidId);
	Body->SetNumberField(TEXT("amount"), ExtractAmount);

	FString BodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	// Capture member vars for lambda
	FString CapturedId = AsteroidId;
	TWeakObjectPtr<ATartariaAsteroid> WeakSelf(this);

	FOnApiResponse CB;
	CB.BindLambda([WeakSelf, CapturedId](bool bOk, const FString& Resp)
	{
		TSharedPtr<FJsonObject> Json;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Resp);
		bool bParsed = FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid();

		bool bSuccess = bOk && bParsed && Json->GetBoolField(TEXT("success"));
		float AmountMined = 0.f;
		float Remaining = 0.f;
		FString Message;

		if (bParsed)
		{
			double DblMined = 0.0, DblRemain = 0.0;
			// Extract total mined from the "extracted" sub-object (sum all resources)
			const TSharedPtr<FJsonObject>* ExtractedObj = nullptr;
			if (Json->TryGetObjectField(TEXT("extracted"), ExtractedObj) && ExtractedObj)
			{
				for (auto& KV : (*ExtractedObj)->Values)
				{
					double Val = 0.0;
					KV.Value->TryGetNumber(Val);
					DblMined += Val;
				}
			}
			Json->TryGetNumberField(TEXT("remaining_yield"), DblRemain);
			Json->TryGetStringField(TEXT("message"), Message);
			AmountMined = (float)DblMined;
			Remaining = (float)DblRemain;
		}
		else if (!bOk)
		{
			Message = TEXT("Network error — could not reach mining backend");
		}
		else
		{
			Message = TEXT("Server returned invalid response");
		}

		UE_LOG(LogTemp, Log, TEXT("TartariaAsteroid [%s]: bSuccess=%d mined=%.1f remaining=%.1f"),
			*CapturedId, bSuccess ? 1 : 0, AmountMined, Remaining);

		AsyncTask(ENamedThreads::GameThread, [WeakSelf, bSuccess, AmountMined, Remaining, Message]()
		{
			ATartariaAsteroid* Self = WeakSelf.Get();
			if (!Self) return;
			Self->OnMineResult(bSuccess, AmountMined, Remaining, Message);
		});
	});

	GI->ApiClient->Post(TEXT("/api/game/mining/extract"), BodyStr, CB);
}

// ---------------------------------------------------------------------------
// Result handling
// ---------------------------------------------------------------------------

void ATartariaAsteroid::OnMineResult(bool bSuccess, float AmountMined, float Remaining,
	const FString& Message)
{
	if (bSuccess)
	{
		CurrentYield = Remaining;

		// Spawn visual harvest feedback
		SpawnHarvestEffect();

		// Notify Blueprint
		OnMineSuccess(AmountMined, Remaining);

		// Toast success
		FString ToastMsg = FString::Printf(TEXT("+%.1f ore extracted"), AmountMined);
		UHJNotificationWidget::Toast(ToastMsg, EHJNotifType::Success, 2.5f);

		// Dim glow to reflect new depletion
		UpdateVisuals();

		if (Remaining <= 0.f)
		{
			OnFullyDepleted();
		}
	}
	else
	{
		OnMineFailed(Message);
		FString ErrorMsg = Message.IsEmpty()
			? TEXT("Mining failed — check server log")
			: Message;
		UHJNotificationWidget::Toast(ErrorMsg, EHJNotifType::Warning, 3.0f);
	}
}

// ---------------------------------------------------------------------------
// Visual helpers
// ---------------------------------------------------------------------------

void ATartariaAsteroid::UpdateVisuals()
{
	if (!ResourceGlow) return;

	float DepletionRatio = (MaxYield > 0.f)
		? FMath::Clamp(CurrentYield / MaxYield, 0.f, 1.f)
		: 0.f;

	// Intensity falls from 4000 to 200 as asteroid empties
	float NewIntensity = FMath::Lerp(200.f, 4000.f, DepletionRatio);
	ResourceGlow->SetIntensity(NewIntensity);

	// Glow colour shifts from full-type-colour → dark grey as depleted
	FLinearColor FullColour;
	switch (AsteroidType)
	{
	case EAsteroidType::Metallic: FullColour = FLinearColor(1.0f, 0.55f, 0.1f); break;
	case EAsteroidType::Icy:      FullColour = FLinearColor(0.3f, 0.8f, 1.0f); break;
	case EAsteroidType::Rocky:    FullColour = FLinearColor(0.9f, 0.75f, 0.3f); break;
	default:                      FullColour = FLinearColor(0.8f, 0.7f, 0.5f); break;
	}

	FLinearColor DeplColour(0.2f, 0.2f, 0.2f);
	ResourceGlow->SetLightColor(FMath::Lerp(DeplColour, FullColour, DepletionRatio));

	// Hide glow completely when fully depleted
	if (DepletionRatio <= 0.f)
		ResourceGlow->SetVisibility(false);
}

// ---------------------------------------------------------------------------
// Harvest orbs (same pattern as ATartariaResourceNode)
// ---------------------------------------------------------------------------

void ATartariaAsteroid::SpawnHarvestEffect()
{
	UWorld* World = GetWorld();
	if (!World) return;

	FVector BaseLoc = GetActorLocation() + FVector(0.f, 0.f, 60.f);

	// Determine orb colour by asteroid type
	FLinearColor OrbColor;
	switch (AsteroidType)
	{
	case EAsteroidType::Metallic: OrbColor = FLinearColor(0.8f, 0.6f, 0.3f); break;  // gold-orange
	case EAsteroidType::Icy:      OrbColor = FLinearColor(0.5f, 0.9f, 1.0f); break;  // ice blue
	case EAsteroidType::Rocky:    OrbColor = FLinearColor(0.7f, 0.55f, 0.3f); break; // warm brown
	default:                      OrbColor = FLinearColor(0.6f, 0.6f, 0.6f); break;
	}

	int32 OrbCount = FMath::RandRange(3, 6);
	for (int32 i = 0; i < OrbCount; ++i)
	{
		FVector Offset(
			FMath::RandRange(-40.f, 40.f),
			FMath::RandRange(-40.f, 40.f),
			FMath::RandRange(0.f, 30.f));

		AActor* Orb = World->SpawnActor<AActor>(AActor::StaticClass(), BaseLoc + Offset);
		if (!Orb) continue;

		UStaticMeshComponent* OrbMesh = NewObject<UStaticMeshComponent>(Orb);
		OrbMesh->RegisterComponent();
		Orb->SetRootComponent(OrbMesh);

		UStaticMesh* SphereSM = LoadObject<UStaticMesh>(
			nullptr, TEXT("/Engine/BasicShapes/Sphere"));
		if (SphereSM) OrbMesh->SetStaticMesh(SphereSM);

		OrbMesh->SetWorldScale3D(FVector(0.06f));
		OrbMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		UMaterialInstanceDynamic* DynMat = OrbMesh->CreateAndSetMaterialInstanceDynamic(0);
		if (DynMat)
		{
			DynMat->SetVectorParameterValue(TEXT("Color"), OrbColor);
			DynMat->SetVectorParameterValue(TEXT("EmissiveColor"), OrbColor * 4.f);
		}

		UPointLightComponent* Glow = NewObject<UPointLightComponent>(Orb);
		Glow->RegisterComponent();
		Glow->SetupAttachment(OrbMesh);
		Glow->SetIntensity(600.f);
		Glow->SetAttenuationRadius(60.f);
		Glow->SetLightColor(OrbColor);
		Glow->SetCastShadows(false);

		Orb->SetLifeSpan(1.8f);

		// Float upward
		TWeakObjectPtr<AActor> WeakOrb(Orb);
		float Speed = FMath::RandRange(70.f, 140.f);
		FTimerHandle FloatHandle;
		World->GetTimerManager().SetTimer(FloatHandle, [WeakOrb, Speed]()
		{
			AActor* O = WeakOrb.Get();
			if (!O) return;
			FVector Loc = O->GetActorLocation();
			O->SetActorLocation(Loc + FVector(0.f, 0.f, Speed * 0.033f));
			O->SetActorScale3D(O->GetActorScale3D() * 0.96f);
		}, 0.033f, true);
	}
}
