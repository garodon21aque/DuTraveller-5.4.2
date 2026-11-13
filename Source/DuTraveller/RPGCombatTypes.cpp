#include "RPGCombatTypes.h"

DEFINE_LOG_CATEGORY(LogRPGCombat);

bool URPGCombatAbilityDefinition::IsConfigured() const
{
	if (AbilityId.IsNone() || MaximumRank < 1 || MaximumRank > 100 || RequiredPlayerLevel < 1 || SpellPointCostPerRank < 0 ||
		PrerequisiteAbilities.Num() > 32 || BaseDamageByRank.Num() > MaximumRank || !FMath::IsFinite(AttackPowerScale) ||
		!FMath::IsFinite(AbilityPowerScale) || !FMath::IsFinite(ManaCost) || ManaCost < 0.f ||
		!FMath::IsFinite(StaminaCost) || StaminaCost < 0.f || !FMath::IsFinite(Range) || Range < 0.f ||
		!FMath::IsFinite(Cooldown) || Cooldown < 0.f || !FMath::IsFinite(ProjectileSpeed) || ProjectileSpeed < 0.f ||
		!FMath::IsFinite(EffectRadius) || EffectRadius < 0.f || !FMath::IsFinite(EffectDuration) || EffectDuration <= 0.f ||
		!FMath::IsFinite(EffectMagnitude) || EffectMagnitude < 0.f) return false;
	for (const float Damage : BaseDamageByRank) if (!FMath::IsFinite(Damage) || Damage < 0.f) return false;
	for (const FName Prerequisite : PrerequisiteAbilities) if (Prerequisite.IsNone() || Prerequisite == AbilityId) return false;
	return true;
}

float URPGCombatAbilityDefinition::GetBaseDamage(const int32 Rank) const
{
	if (BaseDamageByRank.IsEmpty()) return 0.f;
	return BaseDamageByRank[FMath::Clamp(Rank - 1, 0, BaseDamageByRank.Num() - 1)];
}
