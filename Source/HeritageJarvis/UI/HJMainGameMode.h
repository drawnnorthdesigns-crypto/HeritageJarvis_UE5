#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HJMainGameMode.generated.h"

/**
 * AHJMainGameMode — Game mode for the Main Menu level.
 * Shows loading screen while Flask auto-starts, then spawns CEF dashboard.
 */
UCLASS()
class HERITAGEJARVIS_API AHJMainGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHJMainGameMode();
	virtual void BeginPlay() override;

	UPROPERTY()
	class UHJDashboardWidget* DashboardWidget;

private:
	/** Called when Flask first comes online after auto-launch. */
	UFUNCTION()
	void OnFlaskReady();
};
