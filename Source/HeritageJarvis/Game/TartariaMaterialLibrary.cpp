#include "TartariaMaterialLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

UTartariaMaterialLibrary* UTartariaMaterialLibrary::Instance = nullptr;

UTartariaMaterialLibrary* UTartariaMaterialLibrary::Get()
{
	if (!Instance)
	{
		Instance = NewObject<UTartariaMaterialLibrary>();
		Instance->AddToRoot(); // Prevent GC
		Instance->InitMaterials();
		UE_LOG(LogTemp, Log, TEXT("TartariaMaterialLibrary: Initialized 8 heritage materials"));
	}
	return Instance;
}

void UTartariaMaterialLibrary::InitMaterials()
{
	Materials.SetNum(static_cast<int32>(EHeritageMaterial::MAX));

	// ── 0: Tartarian Bronze ──────────────────────────────────
	// Warm, aged bronze with slight green patina.
	// Use for: building exteriors, structural beams, character armor.
	{
		FHeritageMaterialDef& M = Materials[static_cast<int32>(EHeritageMaterial::TartarianBronze)];
		M.Name = TEXT("Tartarian Bronze");
		M.BaseColor = FLinearColor(0.55f, 0.38f, 0.18f);
		M.Metallic = 0.85f;
		M.Roughness = 0.45f;
		M.EmissiveColor = FLinearColor::Black;
		M.EmissiveIntensity = 0.0f;
		M.Opacity = 1.0f;
	}

	// ── 1: Ancient Stone ─────────────────────────────────────
	// Rough gray-beige stone with high roughness.
	// Use for: walls, foundations, monolith surfaces.
	{
		FHeritageMaterialDef& M = Materials[static_cast<int32>(EHeritageMaterial::AncientStone)];
		M.Name = TEXT("Ancient Stone");
		M.BaseColor = FLinearColor(0.4f, 0.38f, 0.35f);
		M.Metallic = 0.0f;
		M.Roughness = 0.92f;
		M.EmissiveColor = FLinearColor::Black;
		M.EmissiveIntensity = 0.0f;
		M.Opacity = 1.0f;
	}

	// ── 2: Forge Iron ────────────────────────────────────────
	// Dark, slightly warm iron with hammered texture feel.
	// Use for: forge buildings, anvils, weapons, industrial structures.
	{
		FHeritageMaterialDef& M = Materials[static_cast<int32>(EHeritageMaterial::ForgeIron)];
		M.Name = TEXT("Forge Iron");
		M.BaseColor = FLinearColor(0.15f, 0.12f, 0.1f);
		M.Metallic = 0.95f;
		M.Roughness = 0.6f;
		M.EmissiveColor = FLinearColor(0.8f, 0.25f, 0.05f); // Subtle hot-iron glow
		M.EmissiveIntensity = 0.15f;
		M.Opacity = 1.0f;
	}

	// ── 3: Crystalline Quartz ────────────────────────────────
	// Translucent purple-white with inner glow.
	// Use for: crystal nodes, monolith elements, void artifacts.
	{
		FHeritageMaterialDef& M = Materials[static_cast<int32>(EHeritageMaterial::CrystallineQuartz)];
		M.Name = TEXT("Crystalline Quartz");
		M.BaseColor = FLinearColor(0.6f, 0.45f, 0.7f);
		M.Metallic = 0.1f;
		M.Roughness = 0.15f;
		M.EmissiveColor = FLinearColor(0.5f, 0.3f, 0.8f);
		M.EmissiveIntensity = 1.5f;
		M.Opacity = 0.8f;
	}

	// ── 4: Void Corruption ───────────────────────────────────
	// Nearly black with sickly green pulsing emission.
	// Use for: void reach terrain, corrupted structures, enemy surfaces.
	{
		FHeritageMaterialDef& M = Materials[static_cast<int32>(EHeritageMaterial::VoidCorruption)];
		M.Name = TEXT("Void Corruption");
		M.BaseColor = FLinearColor(0.05f, 0.08f, 0.06f);
		M.Metallic = 0.3f;
		M.Roughness = 0.7f;
		M.EmissiveColor = FLinearColor(0.15f, 0.4f, 0.1f);
		M.EmissiveIntensity = 0.8f;
		M.Opacity = 1.0f;
	}

	// ── 5: Golden Trim ───────────────────────────────────────
	// Bright polished gold for decorative elements.
	// Use for: trim strips, window frames, ornamental details, UI accents.
	{
		FHeritageMaterialDef& M = Materials[static_cast<int32>(EHeritageMaterial::GoldenTrim)];
		M.Name = TEXT("Golden Trim");
		M.BaseColor = FLinearColor(0.83f, 0.66f, 0.22f);
		M.Metallic = 1.0f;
		M.Roughness = 0.2f;
		M.EmissiveColor = FLinearColor(0.83f, 0.66f, 0.22f);
		M.EmissiveIntensity = 0.3f;
		M.Opacity = 1.0f;
	}

	// ── 6: Scriptorium Marble ────────────────────────────────
	// Cool white-gray marble with subtle blue tint.
	// Use for: scriptorium floors, columns, archive shelving.
	{
		FHeritageMaterialDef& M = Materials[static_cast<int32>(EHeritageMaterial::ScriptoriumMarble)];
		M.Name = TEXT("Scriptorium Marble");
		M.BaseColor = FLinearColor(0.72f, 0.72f, 0.78f);
		M.Metallic = 0.05f;
		M.Roughness = 0.35f;
		M.EmissiveColor = FLinearColor::Black;
		M.EmissiveIntensity = 0.0f;
		M.Opacity = 1.0f;
	}

	// ── 7: Aetheric Glass ────────────────────────────────────
	// Translucent cyan with strong emissive glow.
	// Use for: visors, energy shields, lab containers, portal rims.
	{
		FHeritageMaterialDef& M = Materials[static_cast<int32>(EHeritageMaterial::AethericGlass)];
		M.Name = TEXT("Aetheric Glass");
		M.BaseColor = FLinearColor(0.1f, 0.5f, 0.6f);
		M.Metallic = 0.15f;
		M.Roughness = 0.1f;
		M.EmissiveColor = FLinearColor(0.15f, 0.7f, 0.85f);
		M.EmissiveIntensity = 3.0f;
		M.Opacity = 0.6f;
	}
}

FHeritageMaterialDef UTartariaMaterialLibrary::GetMaterialDef(EHeritageMaterial Material) const
{
	int32 Idx = static_cast<int32>(Material);
	if (Materials.IsValidIndex(Idx))
		return Materials[Idx];
	return FHeritageMaterialDef();
}

UMaterialInstanceDynamic* UTartariaMaterialLibrary::CreateMaterial(UObject* Outer, EHeritageMaterial Material)
{
	int32 Idx = static_cast<int32>(Material);
	if (!Materials.IsValidIndex(Idx)) return nullptr;

	const FHeritageMaterialDef& Def = Materials[Idx];

	// Create a dynamic material from the default engine material
	UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(
		UMaterial::GetDefaultMaterial(MD_Surface), Outer);
	if (!DynMat) return nullptr;

	// Apply properties via parameter overrides
	DynMat->SetVectorParameterValue(TEXT("Color"), Def.BaseColor);

	if (Def.EmissiveIntensity > 0.0f)
	{
		FLinearColor Emissive = Def.EmissiveColor * Def.EmissiveIntensity;
		DynMat->SetVectorParameterValue(TEXT("Emissive"), Emissive);
		DynMat->SetScalarParameterValue(TEXT("Emissive"), Def.EmissiveIntensity);
	}

	return DynMat;
}

UMaterialInstanceDynamic* UTartariaMaterialLibrary::ApplyMaterial(UStaticMeshComponent* Mesh,
	EHeritageMaterial Material)
{
	if (!Mesh) return nullptr;

	int32 Idx = static_cast<int32>(Material);
	if (!Materials.IsValidIndex(Idx)) return nullptr;

	const FHeritageMaterialDef& Def = Materials[Idx];

	UMaterialInstanceDynamic* DynMat = Mesh->CreateAndSetMaterialInstanceDynamic(0);
	if (!DynMat) return nullptr;

	// Set color parameter (works with default engine material)
	DynMat->SetVectorParameterValue(TEXT("Color"), Def.BaseColor);

	if (Def.EmissiveIntensity > 0.0f)
	{
		DynMat->SetScalarParameterValue(TEXT("Emissive"), Def.EmissiveIntensity);
	}

	// Handle transparency
	if (Def.Opacity < 1.0f)
	{
		Mesh->SetRenderCustomDepth(true);
	}

	return DynMat;
}

TArray<FHeritageMaterialDef> UTartariaMaterialLibrary::GetAllMaterials() const
{
	return Materials;
}
