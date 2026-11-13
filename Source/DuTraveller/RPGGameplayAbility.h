#pragma once

#include "Abilities/GameplayAbility.h"
#include "RPGGameplayAbility.generated.h"

class ARPGCharacter;
class URPGActionComponent;
class URPGCombatAbilityDefinition;

/**
 * Locally predicted GAS entry point for DuTraveller combat definitions.
 * One spec is granted per learned/AI ability; the definition is stored as the
 * spec SourceObject so existing data assets remain the single balance source.
 * Presentation and duration effects predict locally; the server validates and
 * performs authoritative combat execution.
 */
UCLASS()
class DUTRAVELLER_API URPGGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	URPGGameplayAbility();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility,
		bool bWasCancelled) override;

private:
	const URPGCombatAbilityDefinition* DefinitionForHandle(FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo) const;
	ARPGCharacter* AvatarCharacter(const FGameplayAbilityActorInfo* ActorInfo) const;
	UFUNCTION() void CombatExecutionFinished(FGameplayEventData Payload);
	TWeakObjectPtr<URPGActionComponent> BoundActionComponent;
	FName ActiveAbilityId;
	bool bEndingAbility = false;
};
