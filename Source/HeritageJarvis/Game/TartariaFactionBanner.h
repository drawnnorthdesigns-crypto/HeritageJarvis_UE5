#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TartariaFactionBanner.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

/**
 * Banner LOD levels based on faction reputation.
 */
UENUM(BlueprintType)
enum class EBannerLOD : uint8
{
	Plain        UMETA(DisplayName = "Plain"),         // Rep < 30: flat colored slab
	Embroidered  UMETA(DisplayName = "Embroidered"),   // Rep 30-70: added trim + emblem
	Gilded       UMETA(DisplayName = "Gilded"),        // Rep > 70: golden frame + emissive glow
};

/**
 * ATartariaFactionBanner — Faction presence banner on buildings and in territories.
 *
 * Visual LOD based on faction reputation:
 *   Plain (rep < 30): Single colored rectangle on a pole
 *   Embroidered (30-70): Colored banner + trim border + faction emblem cube
 *   Gilded (70+): Gold frame + emissive glow + ornamental finial
 *
 * Place near buildings or in territory zones. Set FactionKey to match
 * ARGENTUM, AUREATE, FERRUM, or OBSIDIAN.
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaFactionBanner : public AActor
{
	GENERATED_BODY()

public:
	ATartariaFactionBanner();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Set the faction and reputation. Rebuilds banner visuals. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Economy")
	void SetFaction(const FString& InFactionKey, float Reputation);

	/** Update just the reputation (rebuilds LOD if threshold crossed). */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Economy")
	void UpdateReputation(float NewReputation);

	/** Faction key (ARGENTUM, AUREATE, FERRUM, OBSIDIAN). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Economy")
	FString FactionKey = TEXT("ARGENTUM");

	/** Current reputation (0-100). */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Economy")
	float Reputation = 0.f;

	/** Current banner LOD. */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Economy")
	EBannerLOD CurrentLOD = EBannerLOD::Plain;

	// Components

	/** Banner pole. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Economy")
	UStaticMeshComponent* PoleMesh;

	/** Main banner cloth (flat cube). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Economy")
	UStaticMeshComponent* BannerCloth;

private:
	/** Trim border (Embroidered+). */
	UPROPERTY()
	UStaticMeshComponent* TrimBorder = nullptr;

	/** Faction emblem (Embroidered+). */
	UPROPERTY()
	UStaticMeshComponent* EmblemMesh = nullptr;

	/** Gold frame (Gilded only). */
	UPROPERTY()
	UStaticMeshComponent* GoldFrame = nullptr;

	/** Ornamental finial at top of pole (Gilded only). */
	UPROPERTY()
	UStaticMeshComponent* Finial = nullptr;

	/** Glow light (Gilded only). */
	UPROPERTY()
	UPointLightComponent* GildedGlow = nullptr;

	/** Get the faction's primary color. */
	FLinearColor GetFactionColor() const;

	/** Build/rebuild LOD components. */
	void RebuildLOD();

	/** Set material color on a mesh component. */
	static void SetColor(UStaticMeshComponent* Mesh, const FLinearColor& Color, float Emissive = 0.f);

	/** Animation time. */
	float AnimTime = 0.f;
};
