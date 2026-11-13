#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "RPGAttributeSet.generated.h"

#define RPG_ATTRIBUTE_ACCESSORS(Name) \
 GAMEPLAYATTRIBUTE_PROPERTY_GETTER(URPGAttributeSet, Name) \
 GAMEPLAYATTRIBUTE_VALUE_GETTER(Name) \
 GAMEPLAYATTRIBUTE_VALUE_SETTER(Name) \
 GAMEPLAYATTRIBUTE_VALUE_INITTER(Name)

/** Shared GAS attributes. Base investment and temporary/equipment modifiers stay separate. */
UCLASS()
class DUTRAVELLER_API URPGAttributeSet : public UAttributeSet
{
 GENERATED_BODY()
public:
 URPGAttributeSet();
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
 virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
 virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
 virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
 static TMap<FGameplayTag, FGameplayAttribute> PrimaryAttributes();
 UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Strength, Category="Attributes")
 FGameplayAttributeData Strength;
 RPG_ATTRIBUTE_ACCESSORS(Strength)
 UFUNCTION() void OnRep_Strength(const FGameplayAttributeData& OldValue);
 UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Intelligence, Category="Attributes")
 FGameplayAttributeData Intelligence;
 RPG_ATTRIBUTE_ACCESSORS(Intelligence)
 UFUNCTION() void OnRep_Intelligence(const FGameplayAttributeData& OldValue);
 UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Vigor, Category="Attributes")
 FGameplayAttributeData Vigor;
 RPG_ATTRIBUTE_ACCESSORS(Vigor)
 UFUNCTION() void OnRep_Vigor(const FGameplayAttributeData& OldValue);
 UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Defense, Category="Attributes")
 FGameplayAttributeData Defense;
 RPG_ATTRIBUTE_ACCESSORS(Defense)
 UFUNCTION() void OnRep_Defense(const FGameplayAttributeData& OldValue);
 UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Resilience, Category="Attributes")
 FGameplayAttributeData Resilience;
 RPG_ATTRIBUTE_ACCESSORS(Resilience)
 UFUNCTION() void OnRep_Resilience(const FGameplayAttributeData& OldValue);
 UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Luck, Category="Attributes")
 FGameplayAttributeData Luck;
 RPG_ATTRIBUTE_ACCESSORS(Luck)
 UFUNCTION() void OnRep_Luck(const FGameplayAttributeData& OldValue);
 UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Health, Category="Attributes")
 FGameplayAttributeData Health;
 RPG_ATTRIBUTE_ACCESSORS(Health)
 UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldValue);
 UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxHealth, Category="Attributes")
 FGameplayAttributeData MaxHealth;
 RPG_ATTRIBUTE_ACCESSORS(MaxHealth)
 UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
 UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Mana, Category="Attributes")
 FGameplayAttributeData Mana;
 RPG_ATTRIBUTE_ACCESSORS(Mana)
 UFUNCTION() void OnRep_Mana(const FGameplayAttributeData& OldValue);
 UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxMana, Category="Attributes")
 FGameplayAttributeData MaxMana;
 RPG_ATTRIBUTE_ACCESSORS(MaxMana)
 UFUNCTION() void OnRep_MaxMana(const FGameplayAttributeData& OldValue);
 UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Stamina, Category="Attributes")
 FGameplayAttributeData Stamina;
 RPG_ATTRIBUTE_ACCESSORS(Stamina)
 UFUNCTION() void OnRep_Stamina(const FGameplayAttributeData& OldValue);
 UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxStamina, Category="Attributes")
 FGameplayAttributeData MaxStamina;
 RPG_ATTRIBUTE_ACCESSORS(MaxStamina)
 UFUNCTION() void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);
 UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Armor, Category="Attributes")
 FGameplayAttributeData Armor;
 RPG_ATTRIBUTE_ACCESSORS(Armor)
 UFUNCTION() void OnRep_Armor(const FGameplayAttributeData& OldValue);
 UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_ArmorPenetration, Category="Attributes")
 FGameplayAttributeData ArmorPenetration;
 RPG_ATTRIBUTE_ACCESSORS(ArmorPenetration)
 UFUNCTION() void OnRep_ArmorPenetration(const FGameplayAttributeData& OldValue);
 UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_AttackPower, Category="Attributes")
 FGameplayAttributeData AttackPower;
 RPG_ATTRIBUTE_ACCESSORS(AttackPower)
 UFUNCTION() void OnRep_AttackPower(const FGameplayAttributeData& OldValue);
 UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_AbilityPower, Category="Attributes")
 FGameplayAttributeData AbilityPower;
 RPG_ATTRIBUTE_ACCESSORS(AbilityPower)
 UFUNCTION() void OnRep_AbilityPower(const FGameplayAttributeData& OldValue);
 UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_BlockChance, Category="Attributes")
 FGameplayAttributeData BlockChance;
 RPG_ATTRIBUTE_ACCESSORS(BlockChance)
 UFUNCTION() void OnRep_BlockChance(const FGameplayAttributeData& OldValue);
 UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_CriticalHitChance, Category="Attributes")
 FGameplayAttributeData CriticalHitChance;
 RPG_ATTRIBUTE_ACCESSORS(CriticalHitChance)
 UFUNCTION() void OnRep_CriticalHitChance(const FGameplayAttributeData& OldValue);
 UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_CriticalHitDamage, Category="Attributes")
 FGameplayAttributeData CriticalHitDamage;
 RPG_ATTRIBUTE_ACCESSORS(CriticalHitDamage)
 UFUNCTION() void OnRep_CriticalHitDamage(const FGameplayAttributeData& OldValue);
 UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_HealthRegeneration, Category="Attributes")
 FGameplayAttributeData HealthRegeneration;
 RPG_ATTRIBUTE_ACCESSORS(HealthRegeneration)
 UFUNCTION() void OnRep_HealthRegeneration(const FGameplayAttributeData& OldValue);
 UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MagicCriticalHitChance, Category="Attributes")
 FGameplayAttributeData MagicCriticalHitChance;
 RPG_ATTRIBUTE_ACCESSORS(MagicCriticalHitChance)
 UFUNCTION() void OnRep_MagicCriticalHitChance(const FGameplayAttributeData& OldValue);
 UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MagicCriticalHitDamage, Category="Attributes")
 FGameplayAttributeData MagicCriticalHitDamage;
 RPG_ATTRIBUTE_ACCESSORS(MagicCriticalHitDamage)
 UFUNCTION() void OnRep_MagicCriticalHitDamage(const FGameplayAttributeData& OldValue);
 UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MagicCriticalHitResistance, Category="Attributes")
 FGameplayAttributeData MagicCriticalHitResistance;
 RPG_ATTRIBUTE_ACCESSORS(MagicCriticalHitResistance)
 UFUNCTION() void OnRep_MagicCriticalHitResistance(const FGameplayAttributeData& OldValue);
 UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MagicPenetration, Category="Attributes")
 FGameplayAttributeData MagicPenetration;
 RPG_ATTRIBUTE_ACCESSORS(MagicPenetration)
 UFUNCTION() void OnRep_MagicPenetration(const FGameplayAttributeData& OldValue);
 UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_ManaRegeneration, Category="Attributes")
 FGameplayAttributeData ManaRegeneration;
 RPG_ATTRIBUTE_ACCESSORS(ManaRegeneration)
 UFUNCTION() void OnRep_ManaRegeneration(const FGameplayAttributeData& OldValue);
 UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_StaminaRegeneration, Category="Attributes")
 FGameplayAttributeData StaminaRegeneration;
 RPG_ATTRIBUTE_ACCESSORS(StaminaRegeneration)
 UFUNCTION() void OnRep_StaminaRegeneration(const FGameplayAttributeData& OldValue);
private:
 void ClampValue(const FGameplayAttribute& Attribute, float& NewValue) const;
};

/** Stable native effect definition; per-item magnitudes are provided by server-created specs. */
UCLASS()
class DUTRAVELLER_API URPGEquipmentStatEffect : public UGameplayEffect
{
 GENERATED_BODY()
public:
 URPGEquipmentStatEffect();
};

/** Configurable instant resource restoration for food/potions; item data supplies magnitudes. */
UCLASS()
class DUTRAVELLER_API URPGConsumableResourceEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	URPGConsumableResourceEffect();
};

UCLASS()
class DUTRAVELLER_API URPGSkillArmorEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	URPGSkillArmorEffect();
};

/** Instant health loss; the executing ability supplies the DataName="Damage" SetByCaller magnitude. */
UCLASS()
class DUTRAVELLER_API URPGCombatDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	URPGCombatDamageEffect();
};

/** Instant resource spend for ability and dodge costs; executing code supplies the SetByCaller magnitudes. */
UCLASS()
class DUTRAVELLER_API URPGAbilityCostEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	URPGAbilityCostEffect();
};

/**
 * Duration-based cooldown marker. Applied on ability commit with a per-ability
 * Cooldown.<Id> tag injected through DynamicGrantedTags, so HUD cooldown
 * observers and HasMatchingGameplayTag checks read one authoritative source.
 */
UCLASS()
class DUTRAVELLER_API URPGCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	URPGCooldownEffect();
};

/**
 * Infinite upkeep applied on GAS initialization: every second each resource
 * gains its matching Regeneration attribute (attribute-sourced magnitudes, so
 * equipment/passives that raise ManaRegeneration scale the tick). Clamping in
 * the attribute set keeps values inside [0, Max].
 */
UCLASS()
class DUTRAVELLER_API URPGResourceRegenEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	URPGResourceRegenEffect();
};

/**
 * Permanent primary -> secondary derivation applied on GAS initialization.
 * Each secondary gains an attribute-sourced additive bonus from the matching
 * primary, so point investment, equipment and travel copies that change a
 * primary immediately re-evaluate the derived combat stats.
 */
UCLASS()
class DUTRAVELLER_API URPGDerivedAttributeEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	URPGDerivedAttributeEffect();
};

