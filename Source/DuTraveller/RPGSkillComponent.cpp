#include "RPGSkillComponent.h"
#include "RPGPlayerState.h"
#include "RPGCharacter.h"
#include "RPGActionComponent.h"
#include "RPGAbilitySystemComponent.h"
#include "RPGCombatTypes.h"
#include "AbilitySystemComponent.h"
#include "RPGAttributeSet.h"
#include "Net/UnrealNetwork.h"

URPGSkillComponent::URPGSkillComponent() { SetIsReplicatedByDefault(true); }
void URPGSkillComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (LearnedAbilities.IsEmpty() && Hotbar.IsEmpty())
	{
		for (const FRPGHotbarSlot& Starter : StarterHotbar)
		{
			if (!Starter.Definition || !Starter.Definition->IsConfigured() || !GetAvailableDefinitions().Contains(Starter.Definition) ||
				Starter.Slot < 0 || Starter.Slot >= HotbarSize) continue;
			if (GetLearnedRank(Starter.Definition->AbilityId) == 0)
			{
				FRPGLearnedAbility Entry; Entry.Definition = Starter.Definition; Entry.Rank = 1; LearnedAbilities.Add(Entry);
			}
			TryEquipHotbar(Starter.Definition, Starter.Slot);
		}
	}
	RefreshGrantedAbilities();
	OnRep_Skills();
}
void URPGSkillComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(URPGSkillComponent, LearnedAbilities, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(URPGSkillComponent, Hotbar, COND_OwnerOnly);
}
int32 URPGSkillComponent::GetLearnedRank(const FName AbilityId) const
{
	const FRPGLearnedAbility* Found = LearnedAbilities.FindByPredicate([&](const FRPGLearnedAbility& Entry){ return Entry.Definition && Entry.Definition->AbilityId == AbilityId; });
	return Found ? Found->Rank : 0;
}
void URPGSkillComponent::RequestLearnAbility(URPGCombatAbilityDefinition* Definition) { if (GetOwner()->HasAuthority()) ClientRequestResult(TryLearnAbility(Definition), FName("Learn")); else ServerLearnAbility(Definition); }
void URPGSkillComponent::RequestEquipHotbar(URPGCombatAbilityDefinition* Definition, int32 Slot) { if (GetOwner()->HasAuthority()) ClientRequestResult(TryEquipHotbar(Definition, Slot), FName("EquipHotbar")); else ServerEquipHotbar(Definition, Slot); }
void URPGSkillComponent::RequestActivateHotbar(int32 Slot, ARPGCharacter* Target)
{
	if (GetOwner()->HasAuthority()) ClientRequestResult(TryActivateHotbar(Slot, Target), FName("Activate"));
	else
	{
		const bool bPredicted = TryActivateHotbar(Slot, Target);
		OnRequestResult.Broadcast(bPredicted, FName("Activate"));
		// A newly joined owner may receive the hotbar before its ability spec. Keep
		// the authoritative RPC as a no-prediction fallback for that short window.
		if (!bPredicted) ServerActivateHotbar(Slot, Target);
	}
}
void URPGSkillComponent::ServerLearnAbility_Implementation(URPGCombatAbilityDefinition* Definition) { ClientRequestResult(TryLearnAbility(Definition), FName("Learn")); }
void URPGSkillComponent::ServerEquipHotbar_Implementation(URPGCombatAbilityDefinition* Definition, int32 Slot) { ClientRequestResult(TryEquipHotbar(Definition, Slot), FName("EquipHotbar")); }
void URPGSkillComponent::ServerActivateHotbar_Implementation(int32 Slot, ARPGCharacter* Target) { ClientRequestResult(TryActivateHotbar(Slot, Target), FName("Activate")); }
void URPGSkillComponent::ClientRequestResult_Implementation(bool bSucceeded, FName Operation) { OnRequestResult.Broadcast(bSucceeded, Operation); }

bool URPGSkillComponent::CanLearnAbility(URPGCombatAbilityDefinition* Definition) const
{
	ARPGPlayerState* PS = Cast<ARPGPlayerState>(GetOwner());
	if (!PS || !Definition || !Definition->IsConfigured() || !GetAvailableDefinitions().Contains(Definition) ||
		PS->LVL < Definition->RequiredPlayerLevel || PS->SpellPoints < Definition->SpellPointCostPerRank) return false;
	for (const FName Required : Definition->PrerequisiteAbilities) if (GetLearnedRank(Required) < 1) return false;
	return GetLearnedRank(Definition->AbilityId) < Definition->MaximumRank;
}

bool URPGSkillComponent::TryLearnAbility(URPGCombatAbilityDefinition* Definition)
{
	ARPGPlayerState* PS = Cast<ARPGPlayerState>(GetOwner());
	if (!PS || !PS->HasAuthority() || !CanLearnAbility(Definition)) return false;
	FRPGLearnedAbility* Existing = LearnedAbilities.FindByPredicate([&](const FRPGLearnedAbility& Entry){ return Entry.Definition == Definition; });
	if (Existing && Existing->Rank >= Definition->MaximumRank) return false;
	PS->SpellPoints -= Definition->SpellPointCostPerRank;
	if (Existing) ++Existing->Rank; else { FRPGLearnedAbility Entry; Entry.Definition = Definition; LearnedAbilities.Add(Entry); }
	RefreshGrantedAbilities();
	RefreshPassiveEffects();
	PS->OnRep_Progression(); PS->ForceNetUpdate(); OnRep_Skills();
	return true;
}
bool URPGSkillComponent::TryEquipHotbar(URPGCombatAbilityDefinition* Definition, const int32 Slot)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Slot < 0 || Slot >= HotbarSize || !Definition ||
		Definition->Kind == ERPGAbilityKind::Passive || !GetAvailableDefinitions().Contains(Definition) || GetLearnedRank(Definition->AbilityId) < 1) return false;
	// Moving an equipped skill swaps the destination back into its old slot.
	// Remove old duplicates as well, so presentation and activation agree.
	const FRPGHotbarSlot* Previous = Hotbar.FindByPredicate([&](const FRPGHotbarSlot& Entry){ return Entry.Definition == Definition; });
	const int32 PreviousSlot = Previous ? Previous->Slot : INDEX_NONE;
	const FRPGHotbarSlot* Destination = Hotbar.FindByPredicate([&](const FRPGHotbarSlot& Entry){ return Entry.Slot == Slot; });
	URPGCombatAbilityDefinition* Displaced = Destination ? Destination->Definition.Get() : nullptr;
	Hotbar.RemoveAll([&](const FRPGHotbarSlot& Entry){ return Entry.Definition == Definition || Entry.Slot == Slot; });
	if (PreviousSlot != INDEX_NONE && PreviousSlot != Slot && Displaced && Displaced != Definition)
	{
		FRPGHotbarSlot Swapped; Swapped.Slot = PreviousSlot; Swapped.Definition = Displaced; Hotbar.Add(Swapped);
	}
	FRPGHotbarSlot Entry; Entry.Slot = Slot; Entry.Definition = Definition; Hotbar.Add(Entry);
	RefreshGrantedAbilities();
	OnRep_Skills(); GetOwner()->ForceNetUpdate(); return true;
}
bool URPGSkillComponent::TryActivateHotbar(const int32 Slot, ARPGCharacter* Target)
{
	ARPGPlayerState* PS = Cast<ARPGPlayerState>(GetOwner());
	const FRPGHotbarSlot* Equipped = Hotbar.FindByPredicate([&](const FRPGHotbarSlot& Entry){ return Entry.Slot == Slot; });
	URPGAbilitySystemComponent* ASC = PS ? Cast<URPGAbilitySystemComponent>(PS->AbilitySystem.Get()) : nullptr;
	const int32 Rank = Equipped && Equipped->Definition ? GetLearnedRank(Equipped->Definition->AbilityId) : 0;
	if (Equipped && Equipped->Definition)
		UE_LOG(LogRPGCombat, Log, TEXT("[GAS] %s activation requested slot=%d rank=%d target=%s"),
		       *Equipped->Definition->AbilityId.ToString(), Slot, Rank, *GetNameSafe(Target));
	if (!ASC || !Equipped || Rank <= 0) return false;
	if (GetOwner()->HasAuthority() &&
		!ASC->EnsureAbilityGranted(Equipped->Definition, Rank, InputTagForSlot(Slot)).IsValid()) return false;
	return ASC->ActivateAbilityByInputTag(InputTagForSlot(Slot), Target);
}
void URPGSkillComponent::CopyProgressTo(URPGSkillComponent* Target) const
{
	// PlayerState travel carries authoritative progress over a fresh state's
	// automatic starter loadout; that loadout must not block the transfer.
	if (!Target || Target == this || !GetOwner() || !GetOwner()->HasAuthority() || !Target->GetOwner() || !Target->GetOwner()->HasAuthority()) return;
	Target->LearnedAbilities = LearnedAbilities; Target->Hotbar = Hotbar; Target->OnRep_Skills();
	Target->RefreshGrantedAbilities();
	Target->RefreshPassiveEffects();
	Target->GetOwner()->ForceNetUpdate();
}
void URPGSkillComponent::OnRep_Skills() { OnSkillsChanged.Broadcast(); }

FGameplayTag URPGSkillComponent::InputTagForSlot(const int32 Slot) const
{
	const FString Name = Slot == 0 ? TEXT("InputTag.LMB") : Slot == 1 ? TEXT("InputTag.RMB")
		: FString::Printf(TEXT("InputTag.%02d"), Slot - 1);
	return FGameplayTag::RequestGameplayTag(FName(Name), false);
}

void URPGSkillComponent::RefreshGrantedAbilities()
{
	ARPGPlayerState* PS = Cast<ARPGPlayerState>(GetOwner());
	URPGAbilitySystemComponent* ASC = PS ? Cast<URPGAbilitySystemComponent>(PS->AbilitySystem.Get()) : nullptr;
	if (!PS || !PS->HasAuthority() || !ASC) return;
	for (const FRPGLearnedAbility& Learned : LearnedAbilities)
	{
		if (!Learned.Definition || Learned.Rank <= 0) continue;
		FGameplayTag InputTag;
		if (const FRPGHotbarSlot* Equipped = Hotbar.FindByPredicate([&](const FRPGHotbarSlot& Entry)
			{ return Entry.Definition == Learned.Definition; }))
			InputTag = InputTagForSlot(Equipped->Slot);
		ASC->EnsureAbilityGranted(Learned.Definition, Learned.Rank, InputTag);
	}
}

void URPGSkillComponent::RefreshPassiveEffects()
{
	ARPGPlayerState* PS = Cast<ARPGPlayerState>(GetOwner());
	if (!PS || !PS->HasAuthority() || !PS->AbilitySystem) return;
	if (PassiveArmorEffect.IsValid()) PS->AbilitySystem->RemoveActiveGameplayEffect(PassiveArmorEffect);
	const int32 Rank = GetLearnedRank(TEXT("Abilities.Passive.HaloOfProtection"));
	if (Rank <= 0) return;
	FGameplayEffectSpec Spec(GetDefault<URPGSkillArmorEffect>(), PS->AbilitySystem->MakeEffectContext(), Rank);
	Spec.SetSetByCallerMagnitude(FName("Duration"), 1.f);
	Spec.SetSetByCallerMagnitude(FName("Armor"), 10.f * Rank);
	Spec.SetDuration(UGameplayEffect::INFINITE_DURATION, true);
	PassiveArmorEffect = PS->AbilitySystem->ApplyGameplayEffectSpecToSelf(Spec);
}

TArray<URPGCombatAbilityDefinition*> URPGSkillComponent::GetAvailableDefinitions() const
{
	TArray<URPGCombatAbilityDefinition*> Result;
	TSet<FName> Seen;
	const auto Add = [&](URPGCombatAbilityDefinition* Definition)
	{
		if (IsValid(Definition) && Definition->IsConfigured() && !Seen.Contains(Definition->AbilityId))
		{ Result.Add(Definition); Seen.Add(Definition->AbilityId); }
	};
	if (!AbilityCatalogue)
	{
		if (AvailableAbilities.Num() > 512) return Result;
		for (URPGCombatAbilityDefinition* Definition : AvailableAbilities) Add(Definition);
		return Result;
	}
	if (AbilityCatalogue->GetRowStruct() != FRPGAbilityCatalogueRow::StaticStruct() || AbilityCatalogue->GetRowMap().Num() > 512)
	{
		UE_LOG(LogTemp, Error, TEXT("Skill catalogue has incompatible row type or excessive rows: %s"), *AbilityCatalogue->GetPathName());
		return Result;
	}
	TArray<FName> Names = AbilityCatalogue->GetRowNames();
	Names.Sort(FNameLexicalLess());
	for (FName Name : Names)
	{
		const FRPGAbilityCatalogueRow* Row = AbilityCatalogue->FindRow<FRPGAbilityCatalogueRow>(Name, TEXT("Skill catalogue"));
		if (Row && Row->Definition && Row->Definition->AbilityId == Name) Add(Row->Definition);
	}
	return Result;
}

TArray<FRPGAbilityInfo> URPGSkillComponent::GetAbilityPresentation() const
{
	TArray<FRPGAbilityInfo> Result;
	const auto Tag = [](const FString& Name) { return FGameplayTag::RequestGameplayTag(FName(Name), false); };
	// Empty-slot events clear stale icons after moving or replacing a skill.
	for (int32 Slot = 0; Slot < HotbarSize; ++Slot)
	{
		if (Hotbar.ContainsByPredicate([Slot](const FRPGHotbarSlot& Entry) { return Entry.Slot == Slot && Entry.Definition; })) continue;
		FRPGAbilityInfo Empty;
		Empty.AbilityTag = Tag(TEXT("Abilities.None"));
		Empty.InputTag = Tag(Slot == 0 ? TEXT("InputTag.LMB") : Slot == 1 ? TEXT("InputTag.RMB") : FString::Printf(TEXT("InputTag.%02d"), Slot - 1));
		Result.Add(Empty);
	}
	for (const URPGCombatAbilityDefinition* Definition : GetAvailableDefinitions())
	{
		if (!Definition || !Definition->IsConfigured()) continue;
		FRPGAbilityInfo Info = Definition->Presentation;
		Info.AbilityTag = Tag(Definition->AbilityId.ToString());
		Info.CooldownTag = URPGActionComponent::CooldownTagForAbility(Definition->AbilityId);
		Info.LevelRequirement = Definition->RequiredPlayerLevel;
		Info.AbilityType = Tag(Definition->Kind == ERPGAbilityKind::Passive ? TEXT("Abilities.Type.Passive") : TEXT("Abilities.Type.Offensive"));
		Info.StatusTag = Tag(GetLearnedRank(Definition->AbilityId) > 0 ? TEXT("Abilities.Status.Unlocked") :
			CanLearnAbility(const_cast<URPGCombatAbilityDefinition*>(Definition)) ? TEXT("Abilities.Status.Eligible") : TEXT("Abilities.Status.Locked"));
		if (const FRPGHotbarSlot* Equipped = Hotbar.FindByPredicate([&](const FRPGHotbarSlot& Entry) { return Entry.Definition == Definition; }))
		{
			Info.StatusTag = Tag(TEXT("Abilities.Status.Equipped"));
			Info.InputTag = Tag(Equipped->Slot == 0 ? TEXT("InputTag.LMB") : Equipped->Slot == 1 ? TEXT("InputTag.RMB") : FString::Printf(TEXT("InputTag.%02d"), Equipped->Slot - 1));
		}
		Result.Add(Info);
	}
	return Result;
}
