#include "HJArtifactActor.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/Character.h"
#include "UI/HJNotificationWidget.h"

AHJArtifactActor::AHJArtifactActor()
{
    PrimaryActorTick.bCanEverTick = true;

    // Root: interaction sphere (200 cm radius)
    InteractSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractSphere"));
    InteractSphere->SetSphereRadius(200.0f);
    InteractSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    RootComponent = InteractSphere;

    // Mesh — assign a sphere or custom mesh in BP_Artifact
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(RootComponent);
    Mesh->SetRelativeScale3D(FVector(0.5f));
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Heritage gold glow
    GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLight"));
    GlowLight->SetupAttachment(Mesh);
    GlowLight->SetIntensity(2000.0f);
    GlowLight->SetAttenuationRadius(400.0f);

    InteractSphere->OnComponentBeginOverlap.AddDynamic(this, &AHJArtifactActor::OnBeginOverlap);
    InteractSphere->OnComponentEndOverlap.AddDynamic(this,   &AHJArtifactActor::OnEndOverlap);
}

void AHJArtifactActor::BeginPlay()
{
    Super::BeginPlay();
    BaseLocation = GetActorLocation();
    GlowLight->SetLightColor(GlowColor);
}

void AHJArtifactActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Smooth vertical bob
    BobTime += DeltaTime * BobFrequency * 2.0f * UE_PI;
    FVector NewLoc = BaseLocation;
    NewLoc.Z += FMath::Sin(BobTime) * BobAmplitude;
    SetActorLocation(NewLoc);
}

// -------------------------------------------------------
// IHJInteractable
// -------------------------------------------------------

void AHJArtifactActor::OnInteract_Implementation(APlayerController* Interactor)
{
    UHJNotificationWidget::Toast(
        FString::Printf(TEXT("Opening: %s"), *ArtifactTitle),
        EHJNotifType::Info
    );
    OnArtifactInteracted(Interactor, LinkedProjectId, LinkedProjectName);
}

FString AHJArtifactActor::GetInteractPrompt_Implementation() const
{
    return FString::Printf(TEXT("[E] %s"), *ArtifactTitle);
}

// -------------------------------------------------------
// Overlap — show / hide interact prompt via notifications
// -------------------------------------------------------

void AHJArtifactActor::OnBeginOverlap(UPrimitiveComponent*, AActor* Other,
                                       UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (Cast<ACharacter>(Other))
    {
        // Show a persistent prompt while inside the sphere
        // Duration 0 = stays until the next notification replaces it
        UHJNotificationWidget::Toast(GetInteractPrompt_Implementation(), EHJNotifType::Info, 0.0f);
    }
}

void AHJArtifactActor::OnEndOverlap(UPrimitiveComponent*, AActor* Other,
                                     UPrimitiveComponent*, int32)
{
    // Nothing — prompt auto-expires or is replaced by the next toast
}
