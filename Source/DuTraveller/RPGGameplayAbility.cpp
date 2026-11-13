#include "RPGGameplayAbility.h"

#include "RPGAbilitySystemComponent.h"
#include "RPGActionComponent.h"
#include "RPGAttributeSet.h"
#include "RPGCharacter.h"
#include "RPGCombatTypes.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"

URPGGameplayAbility::URPGGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;
}

const URPGCombatAbilityDefinition* URPGGameplayAbility::DefinitionForHandle(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	const FGameplayAbilitySpec* Spec = ActorInfo && ActorInfo->AbilitySystemComponent.IsValid()
		? ActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(Handle) : nullptr;
	return Spec ? Cast<URPGCombatAbilityDefinition>(Spec->SourceObject.Get()) : nullptr;
}

ARPGCharacter* URPGGameplayAbility::AvatarCharacter(const FGameplayAbilityActorInfo* ActorInfo) const
{
	return ActorInfo ? Cast<ARPGCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
}

bool URPGGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;
	const URPGCombatAbilityDefinition* Definition = DefinitionForHandle(Handle, ActorInfo);
	ARPGCharacter* Character = AvatarCharacter(ActorInfo);
	const URPGAbilitySystemComponent* ASC = ActorInfo
		? Cast<URPGAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr;
	FGameplayTagContainer BlockedStates;
	for (const FName State : {FName(TEXT("State.Dead")), FName(TEXT("State.Stunned")), FName(TEXT("State.Dodging"))})
		if (const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(State, false); Tag.IsValid()) BlockedStates.AddTag(Tag);
	if (ASC && ASC->HasAnyMatchingGameplayTags(BlockedStates)) return false;
	return Definition && Character && Character->ActionComp && ASC;
}

bool URPGGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	const URPGCombatAbilityDefinition* Definition = DefinitionForHandle(Handle, ActorInfo);
	const ARPGCharacter* Character = AvatarCharacter(ActorInfo);
	const URPGAttributeSet* Stats = Character ? Character->GetRPGAttributes() : nullptr;
	return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags) && Definition && Stats &&
		Stats->GetMana() >= Definition->ManaCost && Stats->GetStamina() >= Definition->StaminaCost;
}

bool URPGGameplayAbility::CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	const URPGCombatAbilityDefinition* Definition = DefinitionForHandle(Handle, ActorInfo);
	const ARPGCharacter* Character = AvatarCharacter(ActorInfo);
	return Super::CheckCooldown(Handle, ActorInfo, OptionalRelevantTags) && Definition && Character && Character->ActionComp &&
		Character->ActionComp->GetRemainingCooldown(Definition->AbilityId) <= 0.f;
}

void URPGGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	const URPGCombatAbilityDefinition* Definition = DefinitionForHandle(Handle, ActorInfo);
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!Definition || !ASC || (Definition->ManaCost <= 0.f && Definition->StaminaCost <= 0.f)) return;
	FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo,
		URPGAbilityCostEffect::StaticClass(), GetAbilityLevel(Handle, ActorInfo));
	if (!Spec.IsValid()) return;
	Spec.Data->SetSetByCallerMagnitude(TEXT("Mana"), -Definition->ManaCost);
	Spec.Data->SetSetByCallerMagnitude(TEXT("Stamina"), -Definition->StaminaCost);
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
}

void URPGGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
	const URPGCombatAbilityDefinition* Definition = DefinitionForHandle(Handle, ActorInfo);
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!Definition || !ASC || Definition->Cooldown <= 0.f) return;
	FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo,
		URPGCooldownEffect::StaticClass(), GetAbilityLevel(Handle, ActorInfo));
	if (!Spec.IsValid()) return;
	Spec.Data->SetDuration(Definition->Cooldown, true);
	if (const FGameplayTag Tag = URPGActionComponent::CooldownTagForAbility(Definition->AbilityId); Tag.IsValid())
		Spec.Data->DynamicGrantedTags.AddTag(Tag);
	const FActiveGameplayEffectHandle CooldownHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
	if (ARPGCharacter* Character = AvatarCharacter(ActorInfo); Character && Character->ActionComp)
		Character->ActionComp->RegisterCooldownHandle(Definition->AbilityId, CooldownHandle);
}

void URPGGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	const URPGCombatAbilityDefinition* Definition = DefinitionForHandle(Handle, ActorInfo);
	ARPGCharacter* Character = AvatarCharacter(ActorInfo);
	ARPGCharacter* Target = TriggerEventData
		? Cast<ARPGCharacter>(const_cast<AActor*>(TriggerEventData->Target.Get())) : nullptr;
	const FGameplayAbilitySpec* Spec = ActorInfo && ActorInfo->AbilitySystemComponent.IsValid()
		? ActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(Handle) : nullptr;
	FString Failure;
	const bool bPredictingClient = ActorInfo && ActorInfo->IsLocallyControlled() && !ActorInfo->IsNetAuthority();
	if (!Definition || !Character || !Character->ActionComp || !Spec ||
		!Character->ActionComp->CanActivateAbility(Definition, Spec->Level, Target, Failure, false, bPredictingClient) ||
		!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	ActiveAbilityId = Definition->AbilityId;
	BoundActionComponent = Character->ActionComp;
	if (!ActorInfo->IsNetAuthority())
	{
		Character->ActionComp->OnAbilityCommitted.Broadcast(const_cast<URPGCombatAbilityDefinition*>(Definition), Target);
		return;
	}
	const FGameplayTag FinishedTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Ability.ExecutionFinished"), false);
	UAbilityTask_WaitGameplayEvent* FinishedTask = FinishedTag.IsValid()
		? UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, FinishedTag, nullptr, true, true) : nullptr;
	if (!FinishedTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	FinishedTask->EventReceived.AddDynamic(this, &ThisClass::CombatExecutionFinished);
	FinishedTask->ReadyForActivation();
	if (!Character->ActionComp->TryActivateAbilityFromGameplayAbility(
		const_cast<URPGCombatAbilityDefinition*>(Definition), Spec ? Spec->Level : 1, Target))
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
}

void URPGGameplayAbility::CombatExecutionFinished(const FGameplayEventData Payload)
{
	if (bEndingAbility || !IsActive() || Payload.Instigator.Get() != GetAvatarActorFromActorInfo()) return;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, Payload.EventMagnitude > 0.5f);
}

void URPGGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const bool bReplicateEndAbility, const bool bWasCancelled)
{
	if (bEndingAbility) return;
	TGuardValue<bool> EndingGuard(bEndingAbility, true);
	if (URPGActionComponent* Action = BoundActionComponent.Get())
	{
		if (bWasCancelled) Action->CancelPendingRelease(TEXT("gameplay ability cancelled"));
	}
	BoundActionComponent.Reset();
	ActiveAbilityId = NAME_None;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
