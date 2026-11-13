#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "RPGAbilityInfo.h"
#include "RPGCombatTypes.generated.h"

/** Targeted combat pipeline diagnostics; Log level shows stage transitions, Verbose shows per-hit detail. */
DECLARE_LOG_CATEGORY_EXTERN(LogRPGCombat, Log, All);

UENUM(BlueprintType)
enum class ERPGAbilityKind : uint8
{
	Melee,
	Spell,
	Utility,
	Passive
};

UENUM(BlueprintType)
enum class ERPGSkillExecution : uint8
{
	Damage, Area, Cone, Heal, Shield, Blink, Root, Fear, Drain, Wall, Chain
};

/** Production, data-driven skill/combat definition. Players may learn any combination whose prerequisites are met. */
UCLASS(BlueprintType)
class DUTRAVELLER_API URPGCombatAbilityDefinition : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName AbilityId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(MultiLine=true)) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) ERPGAbilityKind Kind = ERPGAbilityKind::Melee;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1", ClampMax="100")) int32 MaximumRank = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) int32 RequiredPlayerLevel = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) int32 SpellPointCostPerRank = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> PrerequisiteAbilities;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<float> BaseDamageByRank;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float AttackPowerScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float AbilityPowerScale = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float ManaCost = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float StaminaCost = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float Range = 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float Cooldown = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bRequiresTarget = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bAllowFriendlyFire = false;
	/** Optional Blueprint action executed after validated costs. Keep traversal on the DT AnimBP. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Execution") TSubclassOf<class URPGAction> ActionClass;
	/**
	 * Authored montage event that performs the release. When valid, damage and
	 * projectile launch are deferred until this animation event (or the
	 * authoritative timer at its authored montage position on the server).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Execution") FGameplayTag ReleaseEventTag;
	/** Authoritative projectile spawned at the release event. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Execution") TSubclassOf<class ARPGProjectile> ProjectileClass;
	/** Socket the projectile launches from; falls back to the character forward when missing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Execution") FName ReleaseSocket = FName("hand_r");
	/** Optional per-ability damage effect override; the shared URPGCombatDamageEffect is the default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Execution") TSubclassOf<class UGameplayEffect> DamageEffect;
	/** Launch speed override; <= 0 keeps the projectile class default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Execution", meta=(ClampMin="0")) float ProjectileSpeed = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Execution") ERPGSkillExecution Execution = ERPGSkillExecution::Damage;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Execution", meta=(ClampMin="0")) float EffectRadius = 350.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Execution", meta=(ClampMin="0.1")) float EffectDuration = 4.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Execution", meta=(ClampMin="0")) float EffectMagnitude = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Execution") TObjectPtr<class UNiagaraSystem> ReleaseFX;
	/** Designer-authored icon, background and tags shared by skill menus and HUD slots. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation") FRPGAbilityInfo Presentation;
	UFUNCTION(BlueprintPure) bool IsConfigured() const;
	UFUNCTION(BlueprintPure) float GetBaseDamage(int32 Rank) const;
};

/** Catalogue rows reference executable definitions without duplicating their balance. */
USTRUCT(BlueprintType)
struct FRPGAbilityCatalogueRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<URPGCombatAbilityDefinition> Definition;
};

USTRUCT(BlueprintType)
struct FRPGLearnedAbility
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, SaveGame) TObjectPtr<URPGCombatAbilityDefinition> Definition;
	UPROPERTY(BlueprintReadOnly, SaveGame) int32 Rank = 1;
};

USTRUCT(BlueprintType)
struct FRPGHotbarSlot
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 Slot = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TObjectPtr<URPGCombatAbilityDefinition> Definition;
};
