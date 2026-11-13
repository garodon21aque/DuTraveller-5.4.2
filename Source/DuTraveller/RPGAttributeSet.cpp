#include "RPGAttributeSet.h"
#include "Net/UnrealNetwork.h"

URPGConsumableResourceEffect::URPGConsumableResourceEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	const FGameplayAttribute Resources[] = {URPGAttributeSet::GetHealthAttribute(), URPGAttributeSet::GetManaAttribute(), URPGAttributeSet::GetStaminaAttribute()};
	for (const FGameplayAttribute& Attribute : Resources)
	{
		FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Additive;
		FSetByCallerFloat Magnitude;
		Magnitude.DataName = FName(*Attribute.GetName());
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
	}
}

URPGCombatDamageEffect::URPGCombatDamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = URPGAttributeSet::GetHealthAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat Magnitude;
	Magnitude.DataName = FName("Damage");
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
}

URPGAbilityCostEffect::URPGAbilityCostEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	const FGameplayAttribute Resources[] = {URPGAttributeSet::GetManaAttribute(), URPGAttributeSet::GetStaminaAttribute()};
	for (const FGameplayAttribute& Attribute : Resources)
	{
		FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Additive;
		FSetByCallerFloat Magnitude;
		Magnitude.DataName = FName(*Attribute.GetName());
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
	}
}

URPGCooldownEffect::URPGCooldownEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
}

URPGResourceRegenEffect::URPGResourceRegenEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = 1.f;
	bExecutePeriodicEffectOnApplication = false;
	const TPair<FGameplayAttribute, FGameplayAttribute> Resources[] = {
		{URPGAttributeSet::GetHealthAttribute(), URPGAttributeSet::GetHealthRegenerationAttribute()},
		{URPGAttributeSet::GetManaAttribute(), URPGAttributeSet::GetManaRegenerationAttribute()},
		{URPGAttributeSet::GetStaminaAttribute(), URPGAttributeSet::GetStaminaRegenerationAttribute()},
	};
	for (const auto& Entry : Resources)
	{
		FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Entry.Key;
		Modifier.ModifierOp = EGameplayModOp::Additive;
		FAttributeBasedFloat Magnitude;
		Magnitude.Coefficient = 1.f;
		Magnitude.BackingAttribute = FGameplayEffectAttributeCaptureDefinition(Entry.Value,
			EGameplayEffectAttributeCaptureSource::Source, false);
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
	}
}

namespace
{
void AddAttributeSourcedModifier(UGameplayEffect* Effect, const FGameplayAttribute& Target,
	const FGameplayAttribute& Source, const float Coefficient)
{
	FGameplayModifierInfo& Modifier = Effect->Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = Target;
	Modifier.ModifierOp = EGameplayModOp::Additive;
	FAttributeBasedFloat Magnitude;
	Magnitude.Coefficient = Coefficient;
	Magnitude.BackingAttribute = FGameplayEffectAttributeCaptureDefinition(Source,
		EGameplayEffectAttributeCaptureSource::Source, false);
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
}
}

URPGDerivedAttributeEffect::URPGDerivedAttributeEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAttributeSourcedModifier(this, URPGAttributeSet::GetAttackPowerAttribute(), URPGAttributeSet::GetStrengthAttribute(), 1.f);
	AddAttributeSourcedModifier(this, URPGAttributeSet::GetArmorPenetrationAttribute(), URPGAttributeSet::GetStrengthAttribute(), .25f);
	AddAttributeSourcedModifier(this, URPGAttributeSet::GetAbilityPowerAttribute(), URPGAttributeSet::GetIntelligenceAttribute(), 1.f);
	AddAttributeSourcedModifier(this, URPGAttributeSet::GetMagicPenetrationAttribute(), URPGAttributeSet::GetIntelligenceAttribute(), .25f);
	AddAttributeSourcedModifier(this, URPGAttributeSet::GetMaxManaAttribute(), URPGAttributeSet::GetIntelligenceAttribute(), 5.f);
	AddAttributeSourcedModifier(this, URPGAttributeSet::GetManaRegenerationAttribute(), URPGAttributeSet::GetIntelligenceAttribute(), .1f);
	AddAttributeSourcedModifier(this, URPGAttributeSet::GetMaxHealthAttribute(), URPGAttributeSet::GetVigorAttribute(), 10.f);
	AddAttributeSourcedModifier(this, URPGAttributeSet::GetMaxStaminaAttribute(), URPGAttributeSet::GetVigorAttribute(), 5.f);
	AddAttributeSourcedModifier(this, URPGAttributeSet::GetHealthRegenerationAttribute(), URPGAttributeSet::GetVigorAttribute(), .1f);
	AddAttributeSourcedModifier(this, URPGAttributeSet::GetArmorAttribute(), URPGAttributeSet::GetDefenseAttribute(), 2.f);
	AddAttributeSourcedModifier(this, URPGAttributeSet::GetBlockChanceAttribute(), URPGAttributeSet::GetDefenseAttribute(), .5f);
	AddAttributeSourcedModifier(this, URPGAttributeSet::GetMagicCriticalHitResistanceAttribute(), URPGAttributeSet::GetResilienceAttribute(), .5f);
	AddAttributeSourcedModifier(this, URPGAttributeSet::GetCriticalHitChanceAttribute(), URPGAttributeSet::GetLuckAttribute(), .5f);
	AddAttributeSourcedModifier(this, URPGAttributeSet::GetMagicCriticalHitChanceAttribute(), URPGAttributeSet::GetLuckAttribute(), .5f);
	AddAttributeSourcedModifier(this, URPGAttributeSet::GetCriticalHitDamageAttribute(), URPGAttributeSet::GetLuckAttribute(), .01f);
	AddAttributeSourcedModifier(this, URPGAttributeSet::GetMagicCriticalHitDamageAttribute(), URPGAttributeSet::GetLuckAttribute(), .01f);
}

URPGAttributeSet::URPGAttributeSet()
{
 InitHealth(100.f); InitMaxHealth(100.f);
 InitMana(50.f); InitMaxMana(50.f);
 InitStamina(100.f); InitMaxStamina(100.f);
 InitCriticalHitDamage(1.5f); InitMagicCriticalHitDamage(1.5f);
 InitManaRegeneration(2.f);
 InitStaminaRegeneration(12.f);
}

URPGSkillArmorEffect::URPGSkillArmorEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	FSetByCallerFloat Duration; Duration.DataName = TEXT("Duration");
	DurationMagnitude = FGameplayEffectModifierMagnitude(Duration);
	FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = URPGAttributeSet::GetArmorAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat Amount; Amount.DataName = TEXT("Armor");
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Amount);
}
void URPGAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 DOREPLIFETIME_CONDITION_NOTIFY(URPGAttributeSet, Strength, COND_None, REPNOTIFY_Always);
 DOREPLIFETIME_CONDITION_NOTIFY(URPGAttributeSet, Intelligence, COND_None, REPNOTIFY_Always);
 DOREPLIFETIME_CONDITION_NOTIFY(URPGAttributeSet, Vigor, COND_None, REPNOTIFY_Always);
 DOREPLIFETIME_CONDITION_NOTIFY(URPGAttributeSet, Defense, COND_None, REPNOTIFY_Always);
 DOREPLIFETIME_CONDITION_NOTIFY(URPGAttributeSet, Resilience, COND_None, REPNOTIFY_Always);
 DOREPLIFETIME_CONDITION_NOTIFY(URPGAttributeSet, Luck, COND_None, REPNOTIFY_Always);
 DOREPLIFETIME_CONDITION_NOTIFY(URPGAttributeSet, Health, COND_None, REPNOTIFY_Always);
 DOREPLIFETIME_CONDITION_NOTIFY(URPGAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
 DOREPLIFETIME_CONDITION_NOTIFY(URPGAttributeSet, Mana, COND_None, REPNOTIFY_Always);
 DOREPLIFETIME_CONDITION_NOTIFY(URPGAttributeSet, MaxMana, COND_None, REPNOTIFY_Always);
 DOREPLIFETIME_CONDITION_NOTIFY(URPGAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
 DOREPLIFETIME_CONDITION_NOTIFY(URPGAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
 DOREPLIFETIME_CONDITION_NOTIFY(URPGAttributeSet, Armor, COND_None, REPNOTIFY_Always);
 DOREPLIFETIME_CONDITION_NOTIFY(URPGAttributeSet, ArmorPenetration, COND_None, REPNOTIFY_Always);
 DOREPLIFETIME_CONDITION_NOTIFY(URPGAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
 DOREPLIFETIME_CONDITION_NOTIFY(URPGAttributeSet, AbilityPower, COND_None, REPNOTIFY_Always);
 DOREPLIFETIME_CONDITION_NOTIFY(URPGAttributeSet, BlockChance, COND_None, REPNOTIFY_Always);
 DOREPLIFETIME_CONDITION_NOTIFY(URPGAttributeSet, CriticalHitChance, COND_None, REPNOTIFY_Always);
 DOREPLIFETIME_CONDITION_NOTIFY(URPGAttributeSet, CriticalHitDamage, COND_None, REPNOTIFY_Always);
 DOREPLIFETIME_CONDITION_NOTIFY(URPGAttributeSet, HealthRegeneration, COND_None, REPNOTIFY_Always);
 DOREPLIFETIME_CONDITION_NOTIFY(URPGAttributeSet, MagicCriticalHitChance, COND_None, REPNOTIFY_Always);
 DOREPLIFETIME_CONDITION_NOTIFY(URPGAttributeSet, MagicCriticalHitDamage, COND_None, REPNOTIFY_Always);
 DOREPLIFETIME_CONDITION_NOTIFY(URPGAttributeSet, MagicCriticalHitResistance, COND_None, REPNOTIFY_Always);
 DOREPLIFETIME_CONDITION_NOTIFY(URPGAttributeSet, MagicPenetration, COND_None, REPNOTIFY_Always);
 DOREPLIFETIME_CONDITION_NOTIFY(URPGAttributeSet, ManaRegeneration, COND_None, REPNOTIFY_Always);
 DOREPLIFETIME_CONDITION_NOTIFY(URPGAttributeSet, StaminaRegeneration, COND_None, REPNOTIFY_Always);
}
void URPGAttributeSet::OnRep_Strength(const FGameplayAttributeData& OldValue)
{
 GAMEPLAYATTRIBUTE_REPNOTIFY(URPGAttributeSet, Strength, OldValue);
}
void URPGAttributeSet::OnRep_Intelligence(const FGameplayAttributeData& OldValue)
{
 GAMEPLAYATTRIBUTE_REPNOTIFY(URPGAttributeSet, Intelligence, OldValue);
}
void URPGAttributeSet::OnRep_Vigor(const FGameplayAttributeData& OldValue)
{
 GAMEPLAYATTRIBUTE_REPNOTIFY(URPGAttributeSet, Vigor, OldValue);
}
void URPGAttributeSet::OnRep_Defense(const FGameplayAttributeData& OldValue)
{
 GAMEPLAYATTRIBUTE_REPNOTIFY(URPGAttributeSet, Defense, OldValue);
}
void URPGAttributeSet::OnRep_Resilience(const FGameplayAttributeData& OldValue)
{
 GAMEPLAYATTRIBUTE_REPNOTIFY(URPGAttributeSet, Resilience, OldValue);
}
void URPGAttributeSet::OnRep_Luck(const FGameplayAttributeData& OldValue)
{
 GAMEPLAYATTRIBUTE_REPNOTIFY(URPGAttributeSet, Luck, OldValue);
}
void URPGAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
 GAMEPLAYATTRIBUTE_REPNOTIFY(URPGAttributeSet, Health, OldValue);
}
void URPGAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
 GAMEPLAYATTRIBUTE_REPNOTIFY(URPGAttributeSet, MaxHealth, OldValue);
}
void URPGAttributeSet::OnRep_Mana(const FGameplayAttributeData& OldValue)
{
 GAMEPLAYATTRIBUTE_REPNOTIFY(URPGAttributeSet, Mana, OldValue);
}
void URPGAttributeSet::OnRep_MaxMana(const FGameplayAttributeData& OldValue)
{
 GAMEPLAYATTRIBUTE_REPNOTIFY(URPGAttributeSet, MaxMana, OldValue);
}
void URPGAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldValue)
{
 GAMEPLAYATTRIBUTE_REPNOTIFY(URPGAttributeSet, Stamina, OldValue);
}
void URPGAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue)
{
 GAMEPLAYATTRIBUTE_REPNOTIFY(URPGAttributeSet, MaxStamina, OldValue);
}
void URPGAttributeSet::OnRep_Armor(const FGameplayAttributeData& OldValue)
{
 GAMEPLAYATTRIBUTE_REPNOTIFY(URPGAttributeSet, Armor, OldValue);
}
void URPGAttributeSet::OnRep_ArmorPenetration(const FGameplayAttributeData& OldValue)
{
 GAMEPLAYATTRIBUTE_REPNOTIFY(URPGAttributeSet, ArmorPenetration, OldValue);
}
void URPGAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldValue)
{
 GAMEPLAYATTRIBUTE_REPNOTIFY(URPGAttributeSet, AttackPower, OldValue);
}
void URPGAttributeSet::OnRep_AbilityPower(const FGameplayAttributeData& OldValue)
{
 GAMEPLAYATTRIBUTE_REPNOTIFY(URPGAttributeSet, AbilityPower, OldValue);
}
void URPGAttributeSet::OnRep_BlockChance(const FGameplayAttributeData& OldValue)
{
 GAMEPLAYATTRIBUTE_REPNOTIFY(URPGAttributeSet, BlockChance, OldValue);
}
void URPGAttributeSet::OnRep_CriticalHitChance(const FGameplayAttributeData& OldValue)
{
 GAMEPLAYATTRIBUTE_REPNOTIFY(URPGAttributeSet, CriticalHitChance, OldValue);
}
void URPGAttributeSet::OnRep_CriticalHitDamage(const FGameplayAttributeData& OldValue)
{
 GAMEPLAYATTRIBUTE_REPNOTIFY(URPGAttributeSet, CriticalHitDamage, OldValue);
}
void URPGAttributeSet::OnRep_HealthRegeneration(const FGameplayAttributeData& OldValue)
{
 GAMEPLAYATTRIBUTE_REPNOTIFY(URPGAttributeSet, HealthRegeneration, OldValue);
}
void URPGAttributeSet::OnRep_MagicCriticalHitChance(const FGameplayAttributeData& OldValue)
{
 GAMEPLAYATTRIBUTE_REPNOTIFY(URPGAttributeSet, MagicCriticalHitChance, OldValue);
}
void URPGAttributeSet::OnRep_MagicCriticalHitDamage(const FGameplayAttributeData& OldValue)
{
 GAMEPLAYATTRIBUTE_REPNOTIFY(URPGAttributeSet, MagicCriticalHitDamage, OldValue);
}
void URPGAttributeSet::OnRep_MagicCriticalHitResistance(const FGameplayAttributeData& OldValue)
{
 GAMEPLAYATTRIBUTE_REPNOTIFY(URPGAttributeSet, MagicCriticalHitResistance, OldValue);
}
void URPGAttributeSet::OnRep_MagicPenetration(const FGameplayAttributeData& OldValue)
{
 GAMEPLAYATTRIBUTE_REPNOTIFY(URPGAttributeSet, MagicPenetration, OldValue);
}
void URPGAttributeSet::OnRep_ManaRegeneration(const FGameplayAttributeData& OldValue)
{
 GAMEPLAYATTRIBUTE_REPNOTIFY(URPGAttributeSet, ManaRegeneration, OldValue);
}
void URPGAttributeSet::OnRep_StaminaRegeneration(const FGameplayAttributeData& OldValue)
{
 GAMEPLAYATTRIBUTE_REPNOTIFY(URPGAttributeSet, StaminaRegeneration, OldValue);
}
void URPGAttributeSet::ClampValue(const FGameplayAttribute& Attribute, float& Value) const
{
 Value = FMath::IsFinite(Value) ? FMath::Max(0.f, Value) : 0.f;
 if (Attribute == GetHealthAttribute()) Value = FMath::Min(Value, GetMaxHealth());
 if (Attribute == GetManaAttribute()) Value = FMath::Min(Value, GetMaxMana());
 if (Attribute == GetStaminaAttribute()) Value = FMath::Min(Value, GetMaxStamina());
}
void URPGAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
 Super::PreAttributeChange(Attribute, NewValue);
 ClampValue(Attribute, NewValue);
}
void URPGAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
 Super::PreAttributeBaseChange(Attribute, NewValue);
 ClampValue(Attribute, NewValue);
}
void URPGAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
 Super::PostAttributeChange(Attribute, OldValue, NewValue);
 UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
 if (!ASC || !ASC->IsOwnerActorAuthoritative()) return;
 if (Attribute == GetMaxHealthAttribute() && GetHealth() > NewValue)
  ASC->SetNumericAttributeBase(GetHealthAttribute(), NewValue);
 if (Attribute == GetMaxManaAttribute() && GetMana() > NewValue)
  ASC->SetNumericAttributeBase(GetManaAttribute(), NewValue);
 if (Attribute == GetMaxStaminaAttribute() && GetStamina() > NewValue)
  ASC->SetNumericAttributeBase(GetStaminaAttribute(), NewValue);
}
TMap<FGameplayTag, FGameplayAttribute> URPGAttributeSet::PrimaryAttributes()
{
 TMap<FGameplayTag, FGameplayAttribute> Result;
 if (const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("Attributes.Primary.Strength"), false); Tag.IsValid())
  Result.Add(Tag, GetStrengthAttribute());
 if (const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("Attributes.Primary.Intelligence"), false); Tag.IsValid())
  Result.Add(Tag, GetIntelligenceAttribute());
 if (const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("Attributes.Primary.Vigor"), false); Tag.IsValid())
  Result.Add(Tag, GetVigorAttribute());
 if (const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("Attributes.Primary.Defense"), false); Tag.IsValid())
  Result.Add(Tag, GetDefenseAttribute());
 if (const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("Attributes.Primary.Resilience"), false); Tag.IsValid())
  Result.Add(Tag, GetResilienceAttribute());
 if (const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("Attributes.Primary.Luck"), false); Tag.IsValid())
  Result.Add(Tag, GetLuckAttribute());
 return Result;
}
URPGEquipmentStatEffect::URPGEquipmentStatEffect()
{
	{
		FGameplayModifierInfo& Modifier=Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute=URPGAttributeSet::GetStaminaRegenerationAttribute(); Modifier.ModifierOp=EGameplayModOp::Additive;
		FSetByCallerFloat Amount; Amount.DataName=TEXT("StaminaRegeneration");
		Modifier.ModifierMagnitude=FGameplayEffectModifierMagnitude(Amount);
	}
 DurationPolicy = EGameplayEffectDurationType::Infinite;
 {
  FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
  Modifier.Attribute = URPGAttributeSet::GetStrengthAttribute();
  Modifier.ModifierOp = EGameplayModOp::Additive;
  FSetByCallerFloat Magnitude;
  Magnitude.DataName = FName("Strength");
  Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
 }
 {
  FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
  Modifier.Attribute = URPGAttributeSet::GetIntelligenceAttribute();
  Modifier.ModifierOp = EGameplayModOp::Additive;
  FSetByCallerFloat Magnitude;
  Magnitude.DataName = FName("Intelligence");
  Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
 }
 {
  FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
  Modifier.Attribute = URPGAttributeSet::GetVigorAttribute();
  Modifier.ModifierOp = EGameplayModOp::Additive;
  FSetByCallerFloat Magnitude;
  Magnitude.DataName = FName("Vigor");
  Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
 }
 {
  FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
  Modifier.Attribute = URPGAttributeSet::GetDefenseAttribute();
  Modifier.ModifierOp = EGameplayModOp::Additive;
  FSetByCallerFloat Magnitude;
  Magnitude.DataName = FName("Defense");
  Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
 }
 {
  FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
  Modifier.Attribute = URPGAttributeSet::GetResilienceAttribute();
  Modifier.ModifierOp = EGameplayModOp::Additive;
  FSetByCallerFloat Magnitude;
  Magnitude.DataName = FName("Resilience");
  Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
 }
 {
  FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
  Modifier.Attribute = URPGAttributeSet::GetLuckAttribute();
  Modifier.ModifierOp = EGameplayModOp::Additive;
  FSetByCallerFloat Magnitude;
  Magnitude.DataName = FName("Luck");
  Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
 }
 {
  FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
  Modifier.Attribute = URPGAttributeSet::GetMaxHealthAttribute();
  Modifier.ModifierOp = EGameplayModOp::Additive;
  FSetByCallerFloat Magnitude;
  Magnitude.DataName = FName("MaxHealth");
  Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
 }
 {
  FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
  Modifier.Attribute = URPGAttributeSet::GetMaxManaAttribute();
  Modifier.ModifierOp = EGameplayModOp::Additive;
  FSetByCallerFloat Magnitude;
  Magnitude.DataName = FName("MaxMana");
  Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
 }
 {
  FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
  Modifier.Attribute = URPGAttributeSet::GetMaxStaminaAttribute();
  Modifier.ModifierOp = EGameplayModOp::Additive;
  FSetByCallerFloat Magnitude;
  Magnitude.DataName = FName("MaxStamina");
  Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
 }
 {
  FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
  Modifier.Attribute = URPGAttributeSet::GetArmorAttribute();
  Modifier.ModifierOp = EGameplayModOp::Additive;
  FSetByCallerFloat Magnitude;
  Magnitude.DataName = FName("Armor");
  Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
 }
 {
  FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
  Modifier.Attribute = URPGAttributeSet::GetArmorPenetrationAttribute();
  Modifier.ModifierOp = EGameplayModOp::Additive;
  FSetByCallerFloat Magnitude;
  Magnitude.DataName = FName("ArmorPenetration");
  Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
 }
 {
  FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
  Modifier.Attribute = URPGAttributeSet::GetAttackPowerAttribute();
  Modifier.ModifierOp = EGameplayModOp::Additive;
  FSetByCallerFloat Magnitude;
  Magnitude.DataName = FName("AttackPower");
  Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
 }
 {
  FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
  Modifier.Attribute = URPGAttributeSet::GetAbilityPowerAttribute();
  Modifier.ModifierOp = EGameplayModOp::Additive;
  FSetByCallerFloat Magnitude;
  Magnitude.DataName = FName("AbilityPower");
  Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
 }
 {
  FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
  Modifier.Attribute = URPGAttributeSet::GetBlockChanceAttribute();
  Modifier.ModifierOp = EGameplayModOp::Additive;
  FSetByCallerFloat Magnitude;
  Magnitude.DataName = FName("BlockChance");
  Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
 }
 {
  FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
  Modifier.Attribute = URPGAttributeSet::GetCriticalHitChanceAttribute();
  Modifier.ModifierOp = EGameplayModOp::Additive;
  FSetByCallerFloat Magnitude;
  Magnitude.DataName = FName("CriticalHitChance");
  Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
 }
 {
  FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
  Modifier.Attribute = URPGAttributeSet::GetCriticalHitDamageAttribute();
  Modifier.ModifierOp = EGameplayModOp::Additive;
  FSetByCallerFloat Magnitude;
  Magnitude.DataName = FName("CriticalHitDamage");
  Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
 }
 {
  FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
  Modifier.Attribute = URPGAttributeSet::GetHealthRegenerationAttribute();
  Modifier.ModifierOp = EGameplayModOp::Additive;
  FSetByCallerFloat Magnitude;
  Magnitude.DataName = FName("HealthRegeneration");
  Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
 }
 {
  FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
  Modifier.Attribute = URPGAttributeSet::GetMagicCriticalHitChanceAttribute();
  Modifier.ModifierOp = EGameplayModOp::Additive;
  FSetByCallerFloat Magnitude;
  Magnitude.DataName = FName("MagicCriticalHitChance");
  Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
 }
 {
  FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
  Modifier.Attribute = URPGAttributeSet::GetMagicCriticalHitDamageAttribute();
  Modifier.ModifierOp = EGameplayModOp::Additive;
  FSetByCallerFloat Magnitude;
  Magnitude.DataName = FName("MagicCriticalHitDamage");
  Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
 }
 {
  FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
  Modifier.Attribute = URPGAttributeSet::GetMagicCriticalHitResistanceAttribute();
  Modifier.ModifierOp = EGameplayModOp::Additive;
  FSetByCallerFloat Magnitude;
  Magnitude.DataName = FName("MagicCriticalHitResistance");
  Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
 }
 {
  FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
  Modifier.Attribute = URPGAttributeSet::GetMagicPenetrationAttribute();
  Modifier.ModifierOp = EGameplayModOp::Additive;
  FSetByCallerFloat Magnitude;
  Magnitude.DataName = FName("MagicPenetration");
  Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
 }
 {
  FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
  Modifier.Attribute = URPGAttributeSet::GetManaRegenerationAttribute();
  Modifier.ModifierOp = EGameplayModOp::Additive;
  FSetByCallerFloat Magnitude;
  Magnitude.DataName = FName("ManaRegeneration");
  Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
 }
}

