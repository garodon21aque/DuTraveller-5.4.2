#include "RPGAbilitySystemComponent.h"

#include "RPGActionComponent.h"
#include "RPGCombatTypes.h"
#include "RPGGameplayAbility.h"
#include "RPGCharacter.h"

FGameplayAbilitySpec* URPGAbilitySystemComponent::FindDefinitionSpec(const URPGCombatAbilityDefinition* Definition)
{
	if (!Definition) return nullptr;
	for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
		if (Spec.SourceObject.Get() == Definition && Spec.Ability && Spec.Ability->IsA<URPGGameplayAbility>()) return &Spec;
	return nullptr;
}

void URPGAbilitySystemComponent::SetSpecInputTag(FGameplayAbilitySpec& Spec, const FGameplayTag InputTag)
{
	const FGameplayTag InputRoot = FGameplayTag::RequestGameplayTag(TEXT("InputTag"), false);
	TArray<FGameplayTag> Remove;
	for (const FGameplayTag& Tag : Spec.GetDynamicSpecSourceTags())
		if (InputRoot.IsValid() && Tag.MatchesTag(InputRoot)) Remove.Add(Tag);
	for (const FGameplayTag& Tag : Remove) Spec.GetDynamicSpecSourceTags().RemoveTag(Tag);
	if (InputTag.IsValid()) Spec.GetDynamicSpecSourceTags().AddTag(InputTag);
	MarkAbilitySpecDirty(Spec);
}

FGameplayAbilitySpecHandle URPGAbilitySystemComponent::EnsureAbilityGranted(URPGCombatAbilityDefinition* Definition,
	const int32 Level, const FGameplayTag InputTag)
{
	if (!IsOwnerActorAuthoritative() || !Definition || !Definition->IsConfigured()) return FGameplayAbilitySpecHandle();
	if (FGameplayAbilitySpec* Existing = FindDefinitionSpec(Definition))
	{
		Existing->Level = FMath::Max(1, Level);
		SetSpecInputTag(*Existing, InputTag);
		return Existing->Handle;
	}
	FGameplayAbilitySpec Spec(URPGGameplayAbility::StaticClass(), FMath::Max(1, Level), INDEX_NONE, Definition);
	if (const FGameplayTag AbilityTag = FGameplayTag::RequestGameplayTag(Definition->AbilityId, false); AbilityTag.IsValid())
		Spec.GetDynamicSpecSourceTags().AddTag(AbilityTag);
	if (InputTag.IsValid()) Spec.GetDynamicSpecSourceTags().AddTag(InputTag);
	return GiveAbility(Spec);
}

bool URPGAbilitySystemComponent::ActivateAbilityByInputTag(const FGameplayTag InputTag, ARPGCharacter* Target)
{
	if (!InputTag.IsValid()) return false;
	for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (!Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag)) continue;
		return ActivateSpecWithTarget(Spec.Handle, Target);
	}
	return false;
}

bool URPGAbilitySystemComponent::ActivateAbilityDefinition(URPGCombatAbilityDefinition* Definition, const int32 Level,
	ARPGCharacter* Target)
{
	const FGameplayAbilitySpecHandle Handle = EnsureAbilityGranted(Definition, Level);
	if (!Handle.IsValid()) return false;
	return ActivateSpecWithTarget(Handle, Target);
}

bool URPGAbilitySystemComponent::ActivateSpecWithTarget(const FGameplayAbilitySpecHandle Handle, ARPGCharacter* Target)
{
	if (!Handle.IsValid() || !AbilityActorInfo.IsValid()) return false;
	// Authority callers historically receive a synchronous validation result
	// from TryActivateAbility/TryActivateHotbar. Preserve that contract before
	// GAS starts the spec (TriggerAbilityFromGameplayEvent reports that the spec
	// started even if ActivateAbility immediately cancels after target checks).
	if (IsOwnerActorAuthoritative())
	{
		const FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
		const URPGCombatAbilityDefinition* Definition = Spec
			? Cast<URPGCombatAbilityDefinition>(Spec->SourceObject.Get()) : nullptr;
		ARPGCharacter* Character = Cast<ARPGCharacter>(AbilityActorInfo->AvatarActor.Get());
		FString Failure;
		if (!Spec || !Definition || !Character || !Character->ActionComp ||
			!Character->ActionComp->CanActivateAbility(Definition, Spec->Level, Target, Failure))
			return false;
	}
	FGameplayEventData Payload;
	Payload.EventTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Ability.Activate"), false);
	Payload.Instigator = AbilityActorInfo->AvatarActor.Get();
	Payload.Target = Target;
	return Payload.EventTag.IsValid() &&
		TriggerAbilityFromGameplayEvent(Handle, AbilityActorInfo.Get(), Payload.EventTag, &Payload, *this);
}

bool URPGAbilitySystemComponent::HasGrantedAbility(const FName AbilityId) const
{
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
		if (const URPGCombatAbilityDefinition* Definition = Cast<URPGCombatAbilityDefinition>(Spec.SourceObject.Get()))
			if (Definition->AbilityId == AbilityId) return true;
	return false;
}

bool URPGAbilitySystemComponent::IsGrantedAbilityActive(const FName AbilityId) const
{
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
		if (const URPGCombatAbilityDefinition* Definition = Cast<URPGCombatAbilityDefinition>(Spec.SourceObject.Get()))
			if (Definition->AbilityId == AbilityId) return Spec.IsActive();
	return false;
}

