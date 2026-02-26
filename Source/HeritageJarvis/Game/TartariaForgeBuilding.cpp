#include "TartariaForgeBuilding.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"

ATartariaForgeBuilding::ATartariaForgeBuilding()
{
	PrimaryActorTick.bCanEverTick = false;

	// Building mesh (assign in editor or Blueprint)
	BuildingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BuildingMesh"));
	RootComponent = BuildingMesh;

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
}

FString ATartariaForgeBuilding::GetInteractPrompt_Implementation() const
{
	return FString::Printf(TEXT("Enter %s (Lv.%d)"), *BuildingName, BuildingLevel);
}
