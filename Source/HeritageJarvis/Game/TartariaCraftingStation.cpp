#include "TartariaCraftingStation.h"
#include "Core/HJGameInstance.h"
#include "Core/HJApiClient.h"
#include "Core/TartariaSoundManager.h"
#include "UI/HJNotificationWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/WidgetComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Async/Async.h"
#include "TimerManager.h"

// ---------------------------------------------------------------------------
// Constructor — create components with default transforms
// ---------------------------------------------------------------------------

ATartariaCraftingStation::ATartariaCraftingStation()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.033f; // ~30 fps sufficient for glow pulse

	// ── Primary station mesh ────────────────────────────────────────────
	// Default to a cylinder; BeginPlay() will swap to type-appropriate shape.
	StationMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StationMesh"));
	RootComponent = StationMesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(
		TEXT("/Engine/BasicShapes/Cylinder"));
	if (CylinderFinder.Succeeded())
		StationMesh->SetStaticMesh(CylinderFinder.Object);

	StationMesh->SetWorldScale3D(FVector(1.5f, 1.5f, 1.0f));
	StationMesh->SetCollisionProfileName(TEXT("BlockAll"));

	// ── Ambient glow light ──────────────────────────────────────────────
	StationGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("StationGlow"));
	StationGlow->SetupAttachment(RootComponent);
	StationGlow->SetRelativeLocation(FVector(0.f, 0.f, 80.f));
	StationGlow->SetIntensity(3000.f);
	StationGlow->SetAttenuationRadius(500.f);
	StationGlow->SetCastShadows(false);
	// Default neutral colour — overridden in ApplyStationTypeVisuals()
	StationGlow->SetLightColor(FLinearColor(1.0f, 0.9f, 0.7f));

	// ── Floating name tag ───────────────────────────────────────────────
	NameTag = CreateDefaultSubobject<UTextRenderComponent>(TEXT("NameTag"));
	NameTag->SetupAttachment(RootComponent);
	NameTag->SetRelativeLocation(FVector(0.f, 0.f, 160.f));
	NameTag->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	NameTag->SetText(FText::FromString(TEXT("Crafting Station")));
	NameTag->SetTextRenderColor(FColor(220, 200, 150)); // Parchment gold
	NameTag->SetWorldSize(18.f);
	NameTag->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);

	// ── Recipe panel widget ─────────────────────────────────────────────
	// Positioned in front of the station, facing the player approach direction.
	RecipePanel = CreateDefaultSubobject<UWidgetComponent>(TEXT("RecipePanel"));
	RecipePanel->SetupAttachment(RootComponent);
	RecipePanel->SetRelativeLocation(FVector(-120.f, 0.f, 80.f));
	RecipePanel->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	RecipePanel->SetDrawSize(FVector2D(300.f, 400.f));
	RecipePanel->SetVisibility(false); // Hidden until interact
}

// ---------------------------------------------------------------------------
// BeginPlay — apply visuals and set name tag text
// ---------------------------------------------------------------------------

void ATartariaCraftingStation::BeginPlay()
{
	Super::BeginPlay();
	ApplyStationTypeVisuals();

	if (NameTag && !StationName.IsEmpty())
	{
		NameTag->SetText(FText::FromString(StationName));
	}
}

// ---------------------------------------------------------------------------
// Tick — billboard name tag + glow pulse
// ---------------------------------------------------------------------------

void ATartariaCraftingStation::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ── Billboard name tag to face active camera ───────────────────────
	if (NameTag)
	{
		APlayerController* PC = GetWorld()
			? GetWorld()->GetFirstPlayerController()
			: nullptr;
		if (PC)
		{
			FVector CamLoc;
			FRotator CamRot;
			PC->GetPlayerViewPoint(CamLoc, CamRot);
			FVector ToCamera = (CamLoc - NameTag->GetComponentLocation()).GetSafeNormal();
			FRotator FaceRot = ToCamera.Rotation();
			NameTag->SetWorldRotation(FRotator(0.f, FaceRot.Yaw, 0.f));
		}
	}

	// ── Idle glow pulse ────────────────────────────────────────────────
	if (StationGlow)
	{
		if (bCraftFlashActive)
		{
			// Bright white flash that fades out after a successful craft
			CraftFlashElapsed += DeltaTime;
			float T = FMath::Clamp(CraftFlashElapsed / CraftFlashDuration, 0.f, 1.f);
			float FlashIntensity = FMath::Lerp(20000.f, BaseGlowIntensity, T);
			StationGlow->SetIntensity(FlashIntensity);
			if (CraftFlashElapsed >= CraftFlashDuration)
			{
				bCraftFlashActive = false;
				CraftFlashElapsed = 0.f;
			}
		}
		else
		{
			// Gentle breathing pulse (±15% of base intensity)
			GlowPulseAccumulator += DeltaTime;
			float PulseAlpha = (FMath::Sin(GlowPulseAccumulator * 1.2f) + 1.f) * 0.5f;
			float PulseIntensity = FMath::Lerp(BaseGlowIntensity * 0.85f,
			                                    BaseGlowIntensity * 1.15f, PulseAlpha);
			StationGlow->SetIntensity(PulseIntensity);
		}
	}
}

// ---------------------------------------------------------------------------
// IHJInteractable
// ---------------------------------------------------------------------------

void ATartariaCraftingStation::OnInteract_Implementation(APlayerController* Interactor)
{
	UE_LOG(LogTemp, Log, TEXT("TartariaCraftingStation [%s]: Interact by player"), *StationType);

	UTartariaSoundManager::PlayUIClick(this);

	// Toggle recipe panel
	bPanelOpen = !bPanelOpen;
	if (RecipePanel)
		RecipePanel->SetVisibility(bPanelOpen);

	// Fetch recipes on first open (or always refresh if already fetched)
	if (bPanelOpen)
		FetchRecipes();
}

FString ATartariaCraftingStation::GetInteractPrompt_Implementation() const
{
	return FString::Printf(TEXT("Open %s"), *StationName);
}

// ---------------------------------------------------------------------------
// FetchRecipes — HTTP GET /api/game/crafting/recipes?station_type=X
// ---------------------------------------------------------------------------

void ATartariaCraftingStation::FetchRecipes()
{
	UHJGameInstance* GI = UHJGameInstance::Get(this);
	if (!GI || !GI->ApiClient)
	{
		UE_LOG(LogTemp, Warning, TEXT("TartariaCraftingStation: No ApiClient available"));
		return;
	}

	FString Endpoint = FString::Printf(TEXT("/api/game/crafting/recipes?station_type=%s"),
	                                   *StationType);

	FOnApiResponse CB;
	CB.BindLambda([this](bool bOk, const FString& Resp)
	{
		if (!bOk)
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("TartariaCraftingStation [%s]: FetchRecipes HTTP failed"), *StationType);
			return;
		}

		// Parse JSON response: { "recipes": { "recipe_id": { ... }, ... } }
		TSharedPtr<FJsonObject> Json;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Resp);
		if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("TartariaCraftingStation [%s]: Failed to parse recipe JSON"), *StationType);
			return;
		}

		TArray<FString> RecipeIds;
		const TSharedPtr<FJsonObject>* RecipesObj;
		if (Json->TryGetObjectField(TEXT("recipes"), RecipesObj) && RecipesObj)
		{
			for (auto& Pair : (*RecipesObj)->Values)
			{
				RecipeIds.Add(Pair.Key);
			}
		}

		// Update cache on game thread so Blueprint events fire correctly
		AsyncTask(ENamedThreads::GameThread, [this, RecipeIds]()
		{
			CachedRecipes = RecipeIds;
			bRecipesFetched = true;
			UE_LOG(LogTemp, Log,
			       TEXT("TartariaCraftingStation [%s]: Fetched %d recipes"),
			       *StationType, RecipeIds.Num());
			OnRecipesFetched(RecipeIds);
		});
	});

	GI->ApiClient->Get(Endpoint, CB);
}

// ---------------------------------------------------------------------------
// SubmitCraft — HTTP POST /api/game/crafting/craft-at-station
// ---------------------------------------------------------------------------

void ATartariaCraftingStation::SubmitCraft(const FString& RecipeId)
{
	UHJGameInstance* GI = UHJGameInstance::Get(this);
	if (!GI || !GI->ApiClient)
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("TartariaCraftingStation: No ApiClient — cannot submit craft"));
		return;
	}

	UE_LOG(LogTemp, Log,
	       TEXT("TartariaCraftingStation [%s]: SubmitCraft '%s'"), *StationType, *RecipeId);

	// Build JSON body
	TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject());
	Body->SetStringField(TEXT("recipe_id"), RecipeId);
	Body->SetStringField(TEXT("station_type"), StationType);

	FString BodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	FOnApiResponse CB;
	CB.BindLambda([this, RecipeId](bool bOk, const FString& Resp)
	{
		bool bSuccess = false;
		FString ItemName = RecipeId;
		FString Rarity = TEXT("Common");

		if (bOk)
		{
			TSharedPtr<FJsonObject> Json;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Resp);
			if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
			{
				Json->TryGetBoolField(TEXT("success"), bSuccess);
				FString ParsedName;
				if (Json->TryGetStringField(TEXT("item_name"), ParsedName))
					ItemName = ParsedName;
				FString ParsedRarity;
				if (Json->TryGetStringField(TEXT("rarity"), ParsedRarity))
					Rarity = ParsedRarity;
			}
		}

		// Dispatch result back to game thread
		AsyncTask(ENamedThreads::GameThread, [this, bSuccess, ItemName, Rarity]()
		{
			OnCraftComplete(bSuccess, ItemName);
			OnCraftFinished(bSuccess, ItemName, Rarity);
		});
	});

	GI->ApiClient->Post(TEXT("/api/game/crafting/craft-at-station"), BodyStr, CB);
}

// ---------------------------------------------------------------------------
// OnCraftComplete — feedback: sound + toast + VFX burst
// ---------------------------------------------------------------------------

void ATartariaCraftingStation::OnCraftComplete(bool bSuccess, const FString& ItemName)
{
	if (bSuccess)
	{
		UTartariaSoundManager::PlaySuccess(this);

		FString Msg = FString::Printf(TEXT("Crafted: %s"), *ItemName);
		UHJNotificationWidget::Toast(Msg, EHJNotifType::Success, 3.0f);

		// Trigger glow flash and particle VFX
		bCraftFlashActive = true;
		CraftFlashElapsed = 0.f;
		SpawnCraftVFX();

		UE_LOG(LogTemp, Log,
		       TEXT("TartariaCraftingStation [%s]: Craft succeeded — %s"),
		       *StationType, *ItemName);
	}
	else
	{
		UTartariaSoundManager::PlayError(this);

		FString ErrMsg = FString::Printf(TEXT("Cannot craft: %s"), *ItemName);
		UHJNotificationWidget::Toast(ErrMsg, EHJNotifType::Error, 3.0f);

		UE_LOG(LogTemp, Warning,
		       TEXT("TartariaCraftingStation [%s]: Craft failed — %s"),
		       *StationType, *ItemName);
	}
}

// ---------------------------------------------------------------------------
// ApplyStationTypeVisuals — mesh scale + colour per station type
// ---------------------------------------------------------------------------

void ATartariaCraftingStation::ApplyStationTypeVisuals()
{
	if (!StationMesh || !StationGlow) return;

	if (StationType == TEXT("anvil"))
	{
		// Dark iron anvil: squat cylinder base + rectangular top block silhouette
		// We use a compressed cylinder for the body and rely on the dark material.
		StationMesh->SetRelativeScale3D(FVector(1.0f, 1.8f, 0.6f)); // Wide and low
		BaseGlowIntensity = 4000.f;
		StationGlow->SetLightColor(FLinearColor(1.0f, 0.45f, 0.05f)); // Deep orange
		StationGlow->SetIntensity(BaseGlowIntensity);

		// Dark iron tint via dynamic material
		UMaterialInstanceDynamic* DynMat =
			StationMesh->CreateAndSetMaterialInstanceDynamic(0);
		if (DynMat)
		{
			// Near-black iron colour
			DynMat->SetVectorParameterValue(TEXT("Color"),
				FLinearColor(0.06f, 0.06f, 0.07f));
			DynMat->SetScalarParameterValue(TEXT("Metallic"), 0.95f);
			DynMat->SetScalarParameterValue(TEXT("Roughness"), 0.5f);
		}

		NameTag->SetTextRenderColor(FColor(255, 130, 40)); // Orange label
	}
	else if (StationType == TEXT("alembic"))
	{
		// Glass-like alembic: tall, narrow cylinder (flask shape)
		StationMesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 2.0f));
		BaseGlowIntensity = 3500.f;
		// Alternating green/purple represented as cyan-lavender blend
		StationGlow->SetLightColor(FLinearColor(0.4f, 0.9f, 0.6f)); // Green-teal
		StationGlow->SetIntensity(BaseGlowIntensity);

		// Semi-transparent glass tint
		UMaterialInstanceDynamic* DynMat =
			StationMesh->CreateAndSetMaterialInstanceDynamic(0);
		if (DynMat)
		{
			DynMat->SetVectorParameterValue(TEXT("Color"),
				FLinearColor(0.25f, 0.8f, 0.55f));
			DynMat->SetScalarParameterValue(TEXT("Metallic"), 0.0f);
			DynMat->SetScalarParameterValue(TEXT("Roughness"), 0.05f); // Glossy glass
		}

		NameTag->SetTextRenderColor(FColor(100, 220, 140)); // Green label
	}
	else
	{
		// "workbench" — default: broad flat cylinder, warm wood look
		StationMesh->SetRelativeScale3D(FVector(2.0f, 2.0f, 0.4f));
		BaseGlowIntensity = 2500.f;
		StationGlow->SetLightColor(FLinearColor(1.0f, 0.9f, 0.75f)); // Warm white
		StationGlow->SetIntensity(BaseGlowIntensity);

		UMaterialInstanceDynamic* DynMat =
			StationMesh->CreateAndSetMaterialInstanceDynamic(0);
		if (DynMat)
		{
			// Warm pine-wood brown
			DynMat->SetVectorParameterValue(TEXT("Color"),
				FLinearColor(0.35f, 0.22f, 0.10f));
			DynMat->SetScalarParameterValue(TEXT("Metallic"), 0.0f);
			DynMat->SetScalarParameterValue(TEXT("Roughness"), 0.85f); // Rough wood
		}

		NameTag->SetTextRenderColor(FColor(220, 190, 130)); // Parchment label
	}

	StationGlow->SetIntensity(BaseGlowIntensity);
}

// ---------------------------------------------------------------------------
// SpawnCraftVFX — small orbiting sparks on craft success
// ---------------------------------------------------------------------------

void ATartariaCraftingStation::SpawnCraftVFX()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Determine VFX colour from station type
	FLinearColor VfxColor(1.0f, 0.85f, 0.3f); // Default gold
	if (StationType == TEXT("anvil"))
		VfxColor = FLinearColor(1.0f, 0.5f, 0.1f); // Orange sparks
	else if (StationType == TEXT("alembic"))
		VfxColor = FLinearColor(0.3f, 1.0f, 0.6f); // Green sparks
	else
		VfxColor = FLinearColor(1.0f, 0.95f, 0.6f); // Warm white sparks

	FVector BaseLoc = GetActorLocation() + FVector(0.f, 0.f, 80.f);

	// Spawn 5 small orbs that drift upward and vanish
	for (int32 i = 0; i < 5; ++i)
	{
		FVector Offset(
			FMath::RandRange(-40.f, 40.f),
			FMath::RandRange(-40.f, 40.f),
			FMath::RandRange(10.f, 30.f)
		);

		AActor* Orb = World->SpawnActor<AActor>(AActor::StaticClass(),
		                                        BaseLoc + Offset, FRotator::ZeroRotator);
		if (!Orb) continue;

		UStaticMeshComponent* OrbMesh = NewObject<UStaticMeshComponent>(Orb);
		OrbMesh->RegisterComponent();
		Orb->SetRootComponent(OrbMesh);

		UStaticMesh* SphereSM = LoadObject<UStaticMesh>(
			nullptr, TEXT("/Engine/BasicShapes/Sphere"));
		if (SphereSM) OrbMesh->SetStaticMesh(SphereSM);

		OrbMesh->SetWorldScale3D(FVector(0.04f));
		OrbMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		UMaterialInstanceDynamic* DynMat =
			OrbMesh->CreateAndSetMaterialInstanceDynamic(0);
		if (DynMat)
		{
			DynMat->SetVectorParameterValue(TEXT("Color"), VfxColor);
			DynMat->SetVectorParameterValue(TEXT("EmissiveColor"), VfxColor * 5.f);
		}

		UPointLightComponent* Glow = NewObject<UPointLightComponent>(Orb);
		Glow->RegisterComponent();
		Glow->SetupAttachment(OrbMesh);
		Glow->SetIntensity(800.f);
		Glow->SetAttenuationRadius(80.f);
		Glow->SetLightColor(VfxColor);
		Glow->SetCastShadows(false);

		Orb->SetLifeSpan(1.2f);

		// Float upward — same tick-rate pattern used by TartariaResourceNode
		TWeakObjectPtr<AActor> WeakOrb(Orb);
		float Speed = FMath::RandRange(100.f, 180.f);
		FTimerHandle FloatTimer;
		World->GetTimerManager().SetTimer(FloatTimer, [WeakOrb, Speed]()
		{
			AActor* O = WeakOrb.Get();
			if (O)
			{
				FVector Loc = O->GetActorLocation();
				O->SetActorLocation(Loc + FVector(0.f, 0.f, Speed * 0.033f));
				FVector Scale = O->GetActorScale3D() * 0.96f;
				O->SetActorScale3D(Scale);
			}
		}, 0.033f, true);
	}
}
