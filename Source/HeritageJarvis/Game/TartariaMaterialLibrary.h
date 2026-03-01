#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "TartariaMaterialLibrary.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/**
 * Heritage Material IDs — indices into the material library.
 */
UENUM(BlueprintType)
enum class EHeritageMaterial : uint8
{
	TartarianBronze     UMETA(DisplayName = "Tartarian Bronze"),
	AncientStone        UMETA(DisplayName = "Ancient Stone"),
	ForgeIron           UMETA(DisplayName = "Forge Iron"),
	CrystallineQuartz   UMETA(DisplayName = "Crystalline Quartz"),
	VoidCorruption      UMETA(DisplayName = "Void Corruption"),
	GoldenTrim          UMETA(DisplayName = "Golden Trim"),
	ScriptoriumMarble   UMETA(DisplayName = "Scriptorium Marble"),
	AethericGlass       UMETA(DisplayName = "Aetheric Glass"),

	MAX                 UMETA(Hidden)
};

/**
 * Material property set for creating dynamic material instances.
 */
USTRUCT(BlueprintType)
struct FHeritageMaterialDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor BaseColor = FLinearColor(0.5f, 0.5f, 0.5f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Metallic = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Roughness = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor EmissiveColor = FLinearColor::Black;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float EmissiveIntensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Opacity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Name;
};

/**
 * UTartariaMaterialLibrary — Singleton library of 8 themed PBR material presets.
 *
 * Provides dynamic material instances for the Tartaria world. Each material
 * has tuned BaseColor, Metallic, Roughness, and optional Emissive properties.
 *
 * Usage:
 *   UTartariaMaterialLibrary::Get()->ApplyMaterial(MeshComp, EHeritageMaterial::TartarianBronze);
 *   UMaterialInstanceDynamic* Mat = UTartariaMaterialLibrary::Get()->CreateMaterial(MeshComp, EHeritageMaterial::ForgeIron);
 */
UCLASS()
class HERITAGEJARVIS_API UTartariaMaterialLibrary : public UObject
{
	GENERATED_BODY()

public:
	/** Get the singleton instance. Creates on first call. */
	static UTartariaMaterialLibrary* Get();

	/** Get the material definition for a material type. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Materials")
	FHeritageMaterialDef GetMaterialDef(EHeritageMaterial Material) const;

	/**
	 * Create and apply a dynamic material instance to a mesh component.
	 * Returns the created MID for further customization.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Materials")
	UMaterialInstanceDynamic* ApplyMaterial(UStaticMeshComponent* Mesh, EHeritageMaterial Material);

	/**
	 * Create a dynamic material instance (does not apply to mesh).
	 * Caller must set it on the mesh manually.
	 */
	UMaterialInstanceDynamic* CreateMaterial(UObject* Outer, EHeritageMaterial Material);

	/** Get all material definitions (for UI/debug). */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Materials")
	TArray<FHeritageMaterialDef> GetAllMaterials() const;

private:
	/** Initialize material definitions. */
	void InitMaterials();

	/** Material definitions indexed by EHeritageMaterial. */
	TArray<FHeritageMaterialDef> Materials;

	/** Singleton instance. */
	static UTartariaMaterialLibrary* Instance;
};
