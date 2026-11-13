#include "RPG_SpellMenuWidgetController.h"
#include "RPGPlayerState.h"
#include "RPGSkillComponent.h"
#include "RPGCombatTypes.h"

namespace
{
FGameplayTag OptionalTag(const FString& Name) { return FGameplayTag::RequestGameplayTag(FName(Name), false); }
FGameplayTag AbilityTag(const URPGCombatAbilityDefinition* Definition) { return Definition ? OptionalTag(Definition->AbilityId.ToString()) : FGameplayTag(); }
FGameplayTag KindTag(const ERPGAbilityKind Kind)
{
	const TCHAR* Names[] = {TEXT("Abilities.Type.Offensive"), TEXT("Abilities.Type.Offensive"), TEXT("Abilities.Type.Offensive"), TEXT("Abilities.Type.Passive")};
	return OptionalTag(Names[static_cast<int32>(Kind)]);
}
int32 SlotIndex(const FGameplayTag& Tag)
{
	const FString Name = Tag.ToString();
	if (Name == TEXT("InputTag.LMB")) return 0;
	if (Name == TEXT("InputTag.RMB")) return 1;
	for (int32 Number = 1; Number <= 10; ++Number)
		if (Name == FString::Printf(TEXT("InputTag.%02d"), Number)) return Number + 1;
	return INDEX_NONE;
}
}

void URPG_SpellMenuWidgetController::BroadcastInitialValues_Implementation() { BindCallbacks(); RefreshValues(); }

void URPG_SpellMenuWidgetController::BindCallbacks()
{
	ARPGPlayerState* PS = Cast<ARPGPlayerState>(PlayerState);
	URPGSkillComponent* Skills = PS ? PS->Skills.Get() : nullptr;
	if (BoundSkills.Get() != Skills)
	{
		if (URPGSkillComponent* Previous = BoundSkills.Get())
		{
			Previous->OnSkillsChanged.RemoveDynamic(this, &URPG_SpellMenuWidgetController::RefreshValues);
			Previous->OnRequestResult.RemoveDynamic(this, &URPG_SpellMenuWidgetController::SkillRequestResult);
		}
		BoundSkills = Skills;
		if (Skills)
		{
			Skills->OnSkillsChanged.AddUniqueDynamic(this, &URPG_SpellMenuWidgetController::RefreshValues);
			Skills->OnRequestResult.AddUniqueDynamic(this, &URPG_SpellMenuWidgetController::SkillRequestResult);
		}
	}
	if (BoundPlayerState.Get() != PS)
	{
		if (ARPGPlayerState* Previous = BoundPlayerState.Get()) Previous->OnProgressionChanged.RemoveDynamic(this, &URPG_SpellMenuWidgetController::RefreshValues);
		BoundPlayerState = PS;
		if (PS) PS->OnProgressionChanged.AddUniqueDynamic(this, &URPG_SpellMenuWidgetController::RefreshValues);
	}
}

void URPG_SpellMenuWidgetController::RefreshValues()
{
	const ARPGPlayerState* PS = Cast<ARPGPlayerState>(PlayerState);
	const URPGSkillComponent* Skills = BoundSkills.Get();
	SpellPointsChanged.Broadcast(PS ? PS->SpellPoints : 0);
	if (!Skills) return;
	for (const FRPGAbilityInfo& Info : Skills->GetAbilityPresentation()) AbilityInfoDelegate.Broadcast(Info);
	if (SelectedDefinition) SpellGlobeSelected(SelectedAbilityTag);
}

void URPG_SpellMenuWidgetController::SpellGlobeSelected(const FGameplayTag& AbilityTag)
{
	BindCallbacks();
	if (AbilityTag != SelectedAbilityTag && bWaitingForEquip && SelectedDefinition)
	{
		StopWaitingForEquipDelegate.Broadcast(KindTag(SelectedDefinition->Kind));
		bWaitingForEquip = false;
	}
	SelectedDefinition = nullptr; SelectedAbilityTag = AbilityTag;
	URPGSkillComponent* Skills = BoundSkills.Get(); ARPGPlayerState* PS = Cast<ARPGPlayerState>(PlayerState);
	if (Skills) for (URPGCombatAbilityDefinition* Definition : Skills->GetAvailableDefinitions())
		if (Definition && ::AbilityTag(Definition) == AbilityTag) { SelectedDefinition = Definition; break; }
	if (!SelectedDefinition || !PS) { SpellGlobeSelectedDelegate.Broadcast(false, false, TEXT(""), TEXT("")); return; }
	const int32 Rank = Skills->GetLearnedRank(SelectedDefinition->AbilityId);
	const bool CanSpend = Skills->CanLearnAbility(SelectedDefinition);
	FString Description = SelectedDefinition->Description.ToString();
	Description += FString::Printf(TEXT("\nRank %d/%d | Level %d | Cost %d skill point(s)\nMana %.0f | Stamina %.0f | Cooldown %.1fs"),
		Rank, SelectedDefinition->MaximumRank, SelectedDefinition->RequiredPlayerLevel, SelectedDefinition->SpellPointCostPerRank,
		SelectedDefinition->ManaCost, SelectedDefinition->StaminaCost, SelectedDefinition->Cooldown);
	for (const FName Required : SelectedDefinition->PrerequisiteAbilities)
		Description += FString::Printf(TEXT("\nRequires %s%s"), *Required.ToString(), Skills->GetLearnedRank(Required) > 0 ? TEXT(" (learned)") : TEXT(""));
	const FString Next = Rank >= SelectedDefinition->MaximumRank ? TEXT("Maximum rank") : FString::Printf(TEXT("Rank %d / %d"), Rank + 1, SelectedDefinition->MaximumRank);
	SpellGlobeSelectedDelegate.Broadcast(CanSpend, Rank > 0 && SelectedDefinition->Kind != ERPGAbilityKind::Passive, Description, Next);
}

void URPG_SpellMenuWidgetController::SpendPointButtonPressed()
{
	if (BoundSkills.IsValid() && SelectedDefinition) BoundSkills->RequestLearnAbility(SelectedDefinition);
}

void URPG_SpellMenuWidgetController::GlobeDeselect()
{
	if (bWaitingForEquip && SelectedDefinition) StopWaitingForEquipDelegate.Broadcast(KindTag(SelectedDefinition->Kind));
	bWaitingForEquip = false; SelectedDefinition = nullptr; SelectedAbilityTag = FGameplayTag();
	SpellGlobeSelectedDelegate.Broadcast(false, false, TEXT(""), TEXT(""));
}

void URPG_SpellMenuWidgetController::EquipButtonPressed()
{
	if (!SelectedDefinition || !BoundSkills.IsValid() || BoundSkills->GetLearnedRank(SelectedDefinition->AbilityId) < 1) return;
	bWaitingForEquip = true; WaitForEquipDelegate.Broadcast(KindTag(SelectedDefinition->Kind));
}

void URPG_SpellMenuWidgetController::SpellRowGlobePressed(const FGameplayTag& SlotTag, const FGameplayTag& AbilityType)
{
	if (!bWaitingForEquip || !SelectedDefinition || !BoundSkills.IsValid() || KindTag(SelectedDefinition->Kind) != AbilityType) return;
	const int32 Slot = SlotIndex(SlotTag); if (Slot == INDEX_NONE) return;
	BoundSkills->RequestEquipHotbar(SelectedDefinition, Slot);
}

void URPG_SpellMenuWidgetController::EquipSelectedToSlot(const int32 Slot)
{
	if (!bWaitingForEquip || !SelectedDefinition || !BoundSkills.IsValid() || Slot < 0 || Slot >= BoundSkills->HotbarSize) return;
	if (SelectedDefinition->Kind == ERPGAbilityKind::Passive) return;
	BoundSkills->RequestEquipHotbar(SelectedDefinition, Slot);
}

void URPG_SpellMenuWidgetController::SkillRequestResult(const bool bSucceeded, const FName Operation)
{
	if (bSucceeded && Operation == FName("EquipHotbar") && SelectedDefinition)
	{
		SpellGlobeReassignedDelegate.Broadcast(SelectedAbilityTag);
		StopWaitingForEquipDelegate.Broadcast(KindTag(SelectedDefinition->Kind)); bWaitingForEquip = false;
	}
	RefreshValues();
}
