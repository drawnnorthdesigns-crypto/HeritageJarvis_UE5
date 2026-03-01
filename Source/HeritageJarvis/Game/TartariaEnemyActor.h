#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TartariaEnemyActor.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemyDefeated, const FString&, EnemyName, int32, RewardCredits);

/**
 * ATartariaEnemyActor — Hostile encounter entity in the Tartaria world.
 * Compound primitive humanoid (red-tinted) that spawns from threat data.
 * Supports visual combat choreography: approach, clash, defeat/victory animations.
 */
UCLASS()
class HERITAGEJARVIS_API ATartariaEnemyActor : public ACharacter
{
	GENERATED_BODY()

public:
	ATartariaEnemyActor();

	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;

	// -------------------------------------------------------
	// Properties
	// -------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Enemy")
	FString EnemyName = TEXT("Unknown Foe");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Enemy")
	int32 EnemyPower = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Enemy")
	int32 RewardCredits = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Enemy")
	FString ScenarioKey;

	/** Difficulty level (1-10) affects size and color intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tartaria|Enemy")
	int32 Difficulty = 3;

	// -------------------------------------------------------
	// Body Components
	// -------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Enemy")
	UStaticMeshComponent* EnemyTorso;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Enemy")
	UStaticMeshComponent* EnemyHead;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Enemy")
	UStaticMeshComponent* EnemyArmL;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Enemy")
	UStaticMeshComponent* EnemyArmR;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Enemy")
	UStaticMeshComponent* EnemyLegL;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Enemy")
	UStaticMeshComponent* EnemyLegR;

	/** Menacing glow behind the enemy. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tartaria|Enemy")
	UPointLightComponent* ThreatGlow;

	// -------------------------------------------------------
	// Combat Choreography
	// -------------------------------------------------------

	/** Begin the visual combat sequence with the player. */
	UFUNCTION(BlueprintCallable, Category = "Tartaria|Enemy")
	void BeginCombatChoreography(bool bPlayerWins);

	/** Is the enemy currently in a combat animation? */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Enemy")
	bool bInCombat = false;

	/** Is the enemy defeated (playing death animation)? */
	UPROPERTY(BlueprintReadOnly, Category = "Tartaria|Enemy")
	bool bDefeated = false;

	UPROPERTY(BlueprintAssignable, Category = "Tartaria|Enemy")
	FOnEnemyDefeated OnEnemyDefeated;

private:
	void SetupEnemyBody();
	void ApplyDifficultyAppearance();
	static void ApplyColorToMesh(UStaticMeshComponent* Mesh, const FLinearColor& Color);

	// Combat animation state
	float CombatTimer = 0.f;
	float CombatDuration = 2.5f;
	bool bPlayerWinsCombat = false;

	// Approach state
	bool bApproaching = false;
	float ApproachTimer = 0.f;

	// Idle animation
	float IdlePhase = 0.f;

	// Death dissolve
	float DeathTimer = 0.f;
	float DeathDuration = 1.5f;
	bool bDying = false;

	// Auto-despawn timer
	float DespawnTimer = 0.f;
};
