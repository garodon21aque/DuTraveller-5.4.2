#pragma once

#include "RPGAbilityInfo.h"
#include "RPGWidgetController.h"
#include "RPG_SpellMenuWidgetController.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FSpellGlobeSelectedSignature,
	bool, bSpendPointsButtonEnabled,
	bool, bEquipButtonEnabled,
	FString, DescriptionString,
	FString, NextLevelDescriptionString);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWaitForEquipSelectionSignature, const FGameplayTag&, AbilityType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSpellGlobeReassignedSignature, const FGameplayTag&, AbilityTag);

/** Native contract for the spell-menu controller used by legacy widgets. */
UCLASS(BlueprintType, Blueprintable)
class DUTRAVELLER_API URPG_SpellMenuWidgetController : public URPGWidgetController
{
	GENERATED_BODY()

public:
	virtual void BroadcastInitialValues_Implementation() override;
	void BindCallbacks();
	UPROPERTY(BlueprintAssignable, Category = "GAS|Spell")
	FOnPlayerStatChangedSignature SpellPointsChanged;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Spell")
	FSpellGlobeSelectedSignature SpellGlobeSelectedDelegate;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Spell")
	FWaitForEquipSelectionSignature WaitForEquipDelegate;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Spell")
	FWaitForEquipSelectionSignature StopWaitingForEquipDelegate;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Spell")
	FSpellGlobeReassignedSignature SpellGlobeReassignedDelegate;

	UFUNCTION(BlueprintCallable, Category = "GAS|Spell")
	void SpellGlobeSelected(const FGameplayTag& AbilityTag);

	UFUNCTION(BlueprintCallable, Category = "GAS|Spell")
	void SpendPointButtonPressed();

	UFUNCTION(BlueprintCallable, Category = "GAS|Spell")
	void GlobeDeselect();

	UFUNCTION(BlueprintCallable, Category = "GAS|Spell")
	void EquipButtonPressed();

	UFUNCTION(BlueprintCallable, Category = "GAS|Spell")
	void SpellRowGlobePressed(const FGameplayTag& SlotTag, const FGameplayTag& AbilityType);
	UFUNCTION(BlueprintPure, Category="GAS|Spell") bool IsWaitingForEquip() const { return bWaitingForEquip; }
	UFUNCTION(BlueprintCallable, Category="GAS|Spell") void EquipSelectedToSlot(int32 Slot);
private:
	TWeakObjectPtr<class URPGSkillComponent> BoundSkills;
	TWeakObjectPtr<class ARPGPlayerState> BoundPlayerState;
	UPROPERTY(Transient) TObjectPtr<class URPGCombatAbilityDefinition> SelectedDefinition;
	FGameplayTag SelectedAbilityTag;
	bool bWaitingForEquip = false;
	UFUNCTION() void RefreshValues();
	UFUNCTION() void SkillRequestResult(bool bSucceeded, FName Operation);
};
