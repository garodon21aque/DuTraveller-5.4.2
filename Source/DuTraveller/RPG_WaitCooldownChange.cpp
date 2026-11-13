#include "RPG_WaitCooldownChange.h"

#include "AbilitySystemComponent.h"

URPG_WaitCooldownChange* URPG_WaitCooldownChange::WaitForCooldownChange(
	UAbilitySystemComponent* AbilitySystemComponent,
	FGameplayTag InCooldownTag)
{
	if (!IsValid(AbilitySystemComponent) || !InCooldownTag.IsValid())
	{
		return nullptr;
	}

	URPG_WaitCooldownChange* Task = NewObject<URPG_WaitCooldownChange>();
	Task->ObservedAbilitySystemComponent = AbilitySystemComponent;
	Task->CooldownTag = InCooldownTag;
	Task->RegisterWithGameInstance(AbilitySystemComponent);

	AbilitySystemComponent->RegisterGameplayTagEvent(
		InCooldownTag,
		EGameplayTagEventType::NewOrRemoved).AddUObject(Task, &URPG_WaitCooldownChange::CooldownTagChanged);
	AbilitySystemComponent->OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(
		Task,
		&URPG_WaitCooldownChange::OnActiveEffectAdded);

	Task->BroadcastCurrentCooldown();
	return Task;
}

void URPG_WaitCooldownChange::EndTask()
{
	if (IsValid(ObservedAbilitySystemComponent))
	{
		ObservedAbilitySystemComponent->RegisterGameplayTagEvent(
			CooldownTag,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		ObservedAbilitySystemComponent->OnActiveGameplayEffectAddedDelegateToSelf.RemoveAll(this);
	}

	SetReadyToDestroy();
	MarkAsGarbage();
}

void URPG_WaitCooldownChange::CooldownTagChanged(const FGameplayTag ChangedTag, int32 NewCount)
{
	if (NewCount == 0)
	{
		CooldownEnd.Broadcast(0.0f);
	}
}

void URPG_WaitCooldownChange::OnActiveEffectAdded(
	UAbilitySystemComponent* TargetAbilitySystemComponent,
	const FGameplayEffectSpec& SpecApplied,
	FActiveGameplayEffectHandle ActiveEffectHandle)
{
	BroadcastCurrentCooldown();
}

void URPG_WaitCooldownChange::BroadcastCurrentCooldown()
{
	if (!IsValid(ObservedAbilitySystemComponent))
	{
		return;
	}

	FGameplayTagContainer CooldownTags;
	CooldownTags.AddTag(CooldownTag);
	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTags);
	const TArray<float> TimesRemaining = ObservedAbilitySystemComponent->GetActiveEffectsTimeRemaining(Query);

	float LongestTimeRemaining = 0.0f;
	for (const float TimeRemaining : TimesRemaining)
	{
		LongestTimeRemaining = FMath::Max(LongestTimeRemaining, TimeRemaining);
	}

	if (LongestTimeRemaining > 0.0f)
	{
		CooldownStart.Broadcast(LongestTimeRemaining);
	}
}
