#pragma once

#include "AbilitySystemComponent.h"
#include "RPGAbilitySystemComponent.generated.h"

class ARPGCharacter;
class URPGCombatAbilityDefinition;

/** ASC helpers for granting data-backed abilities and activating them by input tag. */
UCLASS()
class DUTRAVELLER_API URPGAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
public:
	FGameplayAbilitySpecHandle EnsureAbilityGranted(URPGCombatAbilityDefinition* Definition, int32 Level,
		FGameplayTag InputTag = FGameplayTag());
	bool ActivateAbilityByInputTag(FGameplayTag InputTag, ARPGCharacter* Target);
	bool ActivateAbilityDefinition(URPGCombatAbilityDefinition* Definition, int32 Level, ARPGCharacter* Target);
	bool HasGrantedAbility(FName AbilityId) const;
	bool IsGrantedAbilityActive(FName AbilityId) const;

private:
	FGameplayAbilitySpec* FindDefinitionSpec(const URPGCombatAbilityDefinition* Definition);
	void SetSpecInputTag(FGameplayAbilitySpec& Spec, FGameplayTag InputTag);
	bool ActivateSpecWithTarget(FGameplayAbilitySpecHandle Handle, ARPGCharacter* Target);
};
