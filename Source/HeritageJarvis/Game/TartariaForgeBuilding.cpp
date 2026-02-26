#include "TartariaForgeBuilding.h"
#include "TartariaGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"

ATartariaForgeBuilding::ATartariaForgeBuilding()
{
	PrimaryActorTick.bCanEverTick = false;

	// Building mesh — cube primitive for visibility
	BuildingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BuildingMesh"));
	RootComponent = BuildingMesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(
		TEXT("/Engine/BasicShapes/Cube"));
	if (MeshFinder.Succeeded())
		BuildingMesh->SetStaticMesh(MeshFinder.Object);
	BuildingMesh->SetWorldScale3D(FVector(3.0f, 3.0f, 5.0f));

	// Ambient light for the building
	BuildingLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BuildingLight"));
	BuildingLight->SetupAttachment(RootComponent);
	BuildingLight->SetRelativeLocation(FVector(0.f, 0.f, 300.f));
	BuildingLight->SetIntensity(5000.f);
	BuildingLight->SetLightColor(FLinearColor(1.0f, 0.85f, 0.5f)); // Warm amber
	BuildingLight->SetAttenuationRadius(800.f);
}

void ATartariaForgeBuilding::OnInteract_Implementation(APlayerController* Interactor)
{
	UE_LOG(LogTemp, Log, TEXT("TartariaForgeBuilding: Player interacted with %s (Level %d)"),
		*BuildingName, BuildingLevel);

	// Fire Blueprint event for custom UI handling
	OnBuildingInteracted(Interactor);

	// Open CEF dashboard to building-specific route
	ATartariaGameMode* GM = Cast<ATartariaGameMode>(
		UGameplayStatics::GetGameMode(this));
	if (GM)
	{
		FString Route;
		switch (BuildingType)
		{
		case ETartariaBuildingType::Forge:       Route = TEXT("/game#forge"); break;
		case ETartariaBuildingType::Scriptorium: Route = TEXT("/game#scriptorium"); break;
		case ETartariaBuildingType::Treasury:    Route = TEXT("/game#treasury"); break;
		case ETartariaBuildingType::Barracks:    Route = TEXT("/game#barracks"); break;
		case ETartariaBuildingType::Lab:         Route = TEXT("/game#lab"); break;
		}
		GM->OpenDashboardToRoute(Route);
	}
}

FString ATartariaForgeBuilding::GetInteractPrompt_Implementation() const
{
	return FString::Printf(TEXT("Enter %s (Lv.%d)"), *BuildingName, BuildingLevel);
}
