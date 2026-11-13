#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RPGCombatTypes.h"
#include "GameplayEffectTypes.h"
#include "RPGSkillComponent.generated.h"

class ARPGCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRPGSkillsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRPGSkillRequestResult, bool, bSucceeded, FName, Operation);

/** Owner-replicated hybrid progression and hotbar. Definitions do not impose exclusive classes. */
UCLASS(ClassGroup=(Skills), BlueprintType, meta=(BlueprintSpawnableComponent))
class DUTRAVELLER_API URPGSkillComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	URPGSkillComponent();
	virtual void BeginPlay() override;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DT|Startup Skills") TArray<FRPGHotbarSlot> StarterHotbar;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TArray<TObjectPtr<URPGCombatAbilityDefinition>> AvailableAbilities;
	/** A configured table is authoritative; the inline array remains the legacy fallback. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DT|Skill Catalogue") TObjectPtr<UDataTable> AbilityCatalogue;
	UFUNCTION(BlueprintPure, Category="DT|Skill Catalogue") TArray<URPGCombatAbilityDefinition*> GetAvailableDefinitions() const;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Skills) TArray<FRPGLearnedAbility> LearnedAbilities;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Skills) TArray<FRPGHotbarSlot> Hotbar;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="1", ClampMax="24")) int32 HotbarSize = 12;
	UPROPERTY(BlueprintAssignable) FRPGSkillsChanged OnSkillsChanged;
	UPROPERTY(BlueprintAssignable) FRPGSkillRequestResult OnRequestResult;
	UFUNCTION(BlueprintCallable) void RequestLearnAbility(URPGCombatAbilityDefinition* Definition);
	UFUNCTION(BlueprintCallable) void RequestEquipHotbar(URPGCombatAbilityDefinition* Definition, int32 Slot);
	UFUNCTION(BlueprintCallable) void RequestActivateHotbar(int32 Slot, ARPGCharacter* Target);
	UFUNCTION(BlueprintPure) int32 GetLearnedRank(FName AbilityId) const;
	UFUNCTION(BlueprintPure, Category="DT|Skills") bool CanLearnAbility(URPGCombatAbilityDefinition* Definition) const;
	UFUNCTION(BlueprintPure, Category="DT|Skills") TArray<FRPGAbilityInfo> GetAbilityPresentation() const;
	bool TryLearnAbility(URPGCombatAbilityDefinition* Definition);
	bool TryEquipHotbar(URPGCombatAbilityDefinition* Definition, int32 Slot);
	bool TryActivateHotbar(int32 Slot, ARPGCharacter* Target);
	void CopyProgressTo(URPGSkillComponent* Target) const;
	void RefreshPassiveEffects();
	/** Synchronizes learned ranks and hotbar input tags into authoritative GAS specs. */
	void RefreshGrantedAbilities();
private:
	FGameplayTag InputTagForSlot(int32 Slot) const;
	FActiveGameplayEffectHandle PassiveArmorEffect;
	UFUNCTION(Server, Reliable) void ServerLearnAbility(URPGCombatAbilityDefinition* Definition);
	UFUNCTION(Server, Reliable) void ServerEquipHotbar(URPGCombatAbilityDefinition* Definition, int32 Slot);
	UFUNCTION(Server, Reliable) void ServerActivateHotbar(int32 Slot, ARPGCharacter* Target);
	UFUNCTION(Client, Reliable) void ClientRequestResult(bool bSucceeded, FName Operation);
	UFUNCTION() void OnRep_Skills();
};
