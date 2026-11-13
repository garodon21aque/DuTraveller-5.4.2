#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RPGProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UNiagaraSystem;
class USoundBase;
class URPGCombatAbilityDefinition;
class ARPGCharacter;

/**
 * Authoritative combat projectile. The server spawns it from the committed
 * ability's release event; collision and damage run on authority only.
 * Cosmetic presentation lives on Blueprint children and fires on all clients
 * through the replicated impact state, never through local hit callbacks.
 */
UCLASS(BlueprintType, Blueprintable)
class DUTRAVELLER_API ARPGProjectile : public AActor
{
	GENERATED_BODY()

public:
	ARPGProjectile();
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projectile")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projectile")
	TObjectPtr<UProjectileMovementComponent> Movement;

	/** Called once by the server-side ability component immediately after spawn. */
	void InitializeProjectile(ARPGCharacter* Source, const URPGCombatAbilityDefinition* Definition, float BaseDamage, int32 Rank);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile", meta=(ClampMin="10"))
	float LaunchSpeed = 1400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile", meta=(ClampMin="1"))
	float MaxLifeSeconds = 8.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile|FX", meta=(ClampMin="0.05")) float ImpactFXLifetime = 1.5f;

	/** Looping trail FX attached to the projectile on every machine. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile|FX")
	TObjectPtr<UNiagaraSystem> TrailFX;

	/** One-shot FX spawned at the replicated impact location on every machine. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile|FX")
	TObjectPtr<UNiagaraSystem> ImpactFX;

	/** One-shot sound spawned at the replicated impact location on every machine. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile|FX")
	TObjectPtr<USoundBase> ImpactSound;

	/** Cosmetic hook; runs on every machine when the projectile impacts. */
	UFUNCTION(BlueprintImplementableEvent, Category="Projectile")
	void ImpactPresentation(AActor* HitActor, FVector HitLocation);

	UPROPERTY(BlueprintReadOnly, Category="Projectile")
	FName AbilityId;
	UFUNCTION(BlueprintPure, Category="Projectile|Threat") bool IsLiveThreat() const;
	UFUNCTION(BlueprintPure, Category="Projectile|Threat") ARPGCharacter* GetThreatSource() const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Impacted, Category="Projectile")
	bool bImpacted = false;

	UPROPERTY(BlueprintReadOnly, Replicated, Category="Projectile")
	FVector_NetQuantize ImpactLocation;

	UFUNCTION()
	void OnRep_Impacted();

	void PlayImpactFX(const FVector& Location) const;

	UFUNCTION()
	void OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	                     UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

private:
	UFUNCTION() void OnProjectileStopped(const FHitResult& Hit);
	void StopFlightPresentation();
	TWeakObjectPtr<ARPGCharacter> SourceCharacter;
	TWeakObjectPtr<const URPGCombatAbilityDefinition> Ability;
	float Damage = 0.f;
	int32 Rank = 1;
};
