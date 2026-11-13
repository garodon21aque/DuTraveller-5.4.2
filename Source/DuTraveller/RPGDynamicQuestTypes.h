#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "InventoryComponent.h"
#include "ItemObject.h"
#include "RPGDynamicQuestTypes.generated.h"

UENUM(BlueprintType)
enum class ERPGQuestAudience : uint8 { Personal, Party, Guild, Class, Area };

USTRUCT(BlueprintType)
struct FRPGDynamicObjective
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ObjectiveId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Event;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSubclassOf<AActor> TargetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) int32 RequiredCount = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRequireQuestActor = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bOptional = false;
};

USTRUCT(BlueprintType)
struct FRPGRandomRewardEntry
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSubclassOf<UItemObject> ItemClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) int32 Weight = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) int32 MinimumQuantity = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) int32 MaximumQuantity = 1;
};

/** Authored gameplay data. Defaults are reconstruction choices, not recovered balance. */
UCLASS(BlueprintType)
class DUTRAVELLER_API URPGRandomQuestDefinition : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName QuestId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Title;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(MultiLine=true)) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MinimumLevel = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaximumLevel = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTagContainer RequiredTags;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> PrerequisiteQuests;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> AllowedLocations;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRepeatable = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) ERPGQuestAudience Audience = ERPGQuestAudience::Personal;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FRPGDynamicObjective> Objectives;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FRPGInventoryAmount> GuaranteedRewards;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FRPGRandomRewardEntry> RandomRewards;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="32")) int32 RandomRewardRolls = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRewardsWithoutReplacement = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ExperienceReward = 0.f;
	bool IsConfigured() const;
	bool RollRewards(FRandomStream& Random, TArray<FRPGInventoryAmount>& Out) const;
};

USTRUCT(BlueprintType)
struct FRPGQuestCatalogueRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<URPGRandomQuestDefinition> Definition;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) int32 Weight = 1;
};

USTRUCT(BlueprintType)
struct FRPGWeightedQuest
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<URPGRandomQuestDefinition> Definition;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) int32 Weight = 1;
};

/** Quest groups are explicit selection pools. Prerequisites support authored chains. */
UCLASS(BlueprintType)
class DUTRAVELLER_API URPGQuestGroupDefinition : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName GroupId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) int32 Weight = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) int32 MaximumActivePerPlayer = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FRPGWeightedQuest> Quests;
	/** Table entries take priority over inline entries with the same quest ID. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UDataTable> QuestCatalogue;
	UFUNCTION(BlueprintPure, Category="DT|Quest Catalogue") TArray<FRPGWeightedQuest> GetQuestEntries() const;
};

UENUM(BlueprintType)
enum class ERPGDynamicQuestStatus : uint8 { Active, Complete, Claimed };

USTRUCT(BlueprintType)
struct FRPGObjectiveCredit
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) FGuid SpawnId;
	UPROPERTY(SaveGame) FName ObjectiveId;
};

USTRUCT(BlueprintType)
struct FRPGDynamicQuestRecord
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, SaveGame) FGuid InstanceId;
	UPROPERTY(BlueprintReadOnly, SaveGame) FGuid SourceOfferId;
	UPROPERTY(BlueprintReadOnly, SaveGame) TObjectPtr<URPGRandomQuestDefinition> Definition;
	UPROPERTY(BlueprintReadOnly, SaveGame) FName QuestId;
	UPROPERTY(BlueprintReadOnly, SaveGame) FName GroupId;
	UPROPERTY(BlueprintReadOnly, SaveGame) FName LocationId;
	UPROPERTY(BlueprintReadOnly, SaveGame) TArray<int32> Progress;
	UPROPERTY(SaveGame) TArray<FRPGObjectiveCredit> Credits;
	UPROPERTY(BlueprintReadOnly, SaveGame) TArray<FRPGInventoryAmount> RolledRewards;
	UPROPERTY(BlueprintReadOnly, SaveGame) float ExperienceReward = 0.f;
	UPROPERTY(BlueprintReadOnly, SaveGame) ERPGDynamicQuestStatus Status = ERPGDynamicQuestStatus::Active;
};

USTRUCT(BlueprintType)
struct FRPGDynamicQuestSnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, SaveGame) int32 SchemaVersion = 1;
	UPROPERTY(BlueprintReadOnly, SaveGame) TArray<FRPGDynamicQuestRecord> Records;
};
