#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "RPGPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRPGPlayerLevelChangedSignature, int32, NewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRPGXPPercentChangedSignature, float, NewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRPGProgressionChanged);

class UAbilitySystemComponent;
class URPGAttributeSet;
class URPGAttributeMenuWidgetController;
class URPG_SpellMenuWidgetController;
class URPGDynamicQuestComponent;
class URPGSkillComponent;
class URPGPartyComponent;
class ARPGGuildState;

/** Legacy player-state base carrying level and experience surfaced to HUD widgets. */
UCLASS(BlueprintType, Blueprintable)
class DUTRAVELLER_API ARPGPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ARPGPlayerState();
	virtual void BeginPlay() override;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DT|Starting Progression", meta=(ClampMin="0")) int32 StartingAttributePoints = 5;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DT|Starting Progression", meta=(ClampMin="0")) int32 StartingSkillPoints = 3;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DT|Starting Progression") bool bGrantStartingProgression = true;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* PlayerState) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<URPGAttributeSet> Attributes;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quests")
	TObjectPtr<URPGDynamicQuestComponent> DynamicQuests;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Skills")
	TObjectPtr<URPGSkillComponent> Skills;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Party") TObjectPtr<URPGPartyComponent> Party;
	UPROPERTY(BlueprintReadOnly, Replicated, Category="Guild") TObjectPtr<ARPGGuildState> GuildState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Progression, Category = "Progression")
	int32 LVL = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Progression, Category = "Progression")
	float EXP = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Progression")
	float MaxExperience = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Progression")
	float RequiredXp = 100.f;
	/** Rewards for each level crossed; XP is progress within the current level. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Progression|Level Rewards", meta=(ClampMin="0"))
	int32 AttributePointsPerLevel = 5;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Progression|Level Rewards", meta=(ClampMin="0"))
	int32 SkillPointsPerLevel = 1;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Progression, Category = "Progression")
	int32 AttributePoints = 0;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Progression, Category = "Progression")
	int32 SpellPoints = 0;
	/** Reconstruction defaults; exact original economy remains data to recover. */
	UPROPERTY(EditDefaultsOnly, Category = "Progression", meta=(ClampMin="1"))
	int32 AttributePointCost = 1;
	UPROPERTY(EditDefaultsOnly, Category = "Progression", meta=(ClampMin="0.001"))
	float AttributeIncreasePerPurchase = 1.f;
	UPROPERTY(BlueprintAssignable, Category = "Progression")
	FRPGProgressionChanged OnProgressionChanged;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Progression")
	bool GrantProgressionPoints(int32 NewAttributePoints, int32 NewSpellPoints);
	/** Grants the full amount to this player and, by default, every current party member. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Progression")
	bool GrantExperience(float Amount, bool bShareWithParty = true);
	UFUNCTION(BlueprintPure, Category="Progression")
	float GetExperiencePercent() const;
	UFUNCTION(BlueprintCallable, Category = "Progression")
	void RequestUpgradeAttribute(FGameplayTag AttributeTag);
	bool TryUpgradeAttribute(FGameplayTag AttributeTag);
	UFUNCTION(BlueprintPure, Category="Progression")
	float GetAttributeValue(FName AttributeName, bool bBaseValue = false) const;
	UFUNCTION(Server, Reliable)
	void ServerUpgradeAttribute(FGameplayTag AttributeTag);
	UFUNCTION() void OnRep_Progression();

	UPROPERTY(Transient)
	TObjectPtr<URPGAttributeMenuWidgetController> AttributeWidgetController;
	UPROPERTY(Transient)
	TObjectPtr<URPG_SpellMenuWidgetController> SpellWidgetController;
	UPROPERTY(Transient)
	TObjectPtr<class URPGOverlayWidgetController> OverlayWidgetController;

	UPROPERTY(BlueprintAssignable, Category = "Progression")
	/** One notification per level actually gained; XP/point refreshes and initialization do not fire it. */
	FRPGPlayerLevelChangedSignature OnPlayerLevelChangedDelegate;

	UPROPERTY(BlueprintAssignable, Category = "Progression")
	FRPGXPPercentChangedSignature OnXPPercentChangedDelegate;

	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetLVL() const { return LVL; }
private:
	int32 LastBroadcastLevel = INDEX_NONE;
	UPROPERTY(SaveGame) bool bStartingProgressionGranted = false;
};
