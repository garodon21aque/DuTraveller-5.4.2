#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "ItemObject.h"
#include "RPGQuest.generated.h"

class URPGQuest;
class URPGQuestComponent;

/**
 * Legacy quest type enum. The original was a Blueprint enum asset at
 * /Game/Blueprints/Quest/EQuestTypes; serialized graphs retained members
 * Main/Bonus/Class/Dungeon. The first entries preserve the serialized
 * NewEnumerator identities; the semantic names are DisplayName metadata.
 */
UENUM(BlueprintType)
enum class EQuestTypes : uint8
{
	Main UMETA(DisplayName = "Main"),
	Bonus UMETA(DisplayName = "Bonus"),
	Class UMETA(DisplayName = "Class"),
	Dungeon UMETA(DisplayName = "Dungeon"),
	NewEnumerator4 UMETA(DisplayName = "NewEnumerator4"),
	NewEnumerator5 UMETA(DisplayName = "NewEnumerator5"),
};

/** Legacy objective type enum (serialized asset path kept the original EQObjctiveTypes spelling). */
UENUM(BlueprintType)
enum class EQObjctiveTypes : uint8
{
	NewEnumerator0 UMETA(DisplayName = "NewEnumerator0"),
	NewEnumerator1 UMETA(DisplayName = "NewEnumerator1"),
	NewEnumerator2 UMETA(DisplayName = "NewEnumerator2"),
	NewEnumerator3 UMETA(DisplayName = "NewEnumerator3"),
};

/** Row of quest objectives; reconstructed from serialized pin names in quest widgets. */
USTRUCT(BlueprintType)
struct DUTRAVELLER_API FObjectiveDetails : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	int32 ObjectiveID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText ObjectiveName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText ObjectiveDescription;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	EQObjctiveTypes ObjectiveType = EQObjctiveTypes::NewEnumerator0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	int32 Quantity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	int32 CurrentObjectiveProgress = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	bool IsComplete = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	float BonusXP = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	int32 BonusRenown = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	bool bIsOptional = false;
};

/** One quest stage and its objectives; reconstructed from serialized pin names in quest widgets. */
USTRUCT(BlueprintType)
struct DUTRAVELLER_API FStageDetails : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText StageName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText StageDescription;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	TArray<FObjectiveDetails> Objectives;
};

/** Full quest definition row; reconstructed from serialized pin names in quest widgets and QuestData references. */
USTRUCT(BlueprintType)
struct DUTRAVELLER_API FQuestDetails : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText QuestName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText QuestTitle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText QuestDescription;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText LogDescription;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	EQuestTypes QuestTypes = EQuestTypes::Main;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	TArray<FStageDetails> Stages;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	TArray<FObjectiveDetails> Objectives;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	TArray<TSubclassOf<UItemObject>> ItemRewards;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	TMap<TSubclassOf<UItemObject>, int32> CurrencyRewards;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	float XPReward = 0.f;
};

/** Data-asset wrapper for a quest definition (named by the recovered historical design notes). */
UCLASS(BlueprintType, Blueprintable)
class DUTRAVELLER_API UQuestDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FQuestDetails QuestDetails;
};

/**
 * Runtime quest object. Serialized quest widgets keep `QuestActor`/`Quest`
 * members of this type and read QuestDetails/QuestID/DataTable from it.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class DUTRAVELLER_API URPGQuest : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FName QuestID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FQuestDetails QuestDetails;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	TObjectPtr<UDataTable> DataTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	int32 Stage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	bool bIsComplete = false;

	UFUNCTION(BlueprintCallable, Category = "Quest")
	void IsAllObjectiveComplete(bool& Complete);
};

/** Legacy quest component; parent of the missing BP_QuestLog Blueprint class.
 *  The serialized `CurrentQuests` array held BP_QuestGiver references and is
 *  restored as a Blueprint member variable on BP_QuestLog. */
UCLASS(ClassGroup = (Quests), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class DUTRAVELLER_API URPGQuestComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URPGQuestComponent();

	UFUNCTION(BlueprintCallable, Category = "Quests")
	void AddNewQuest(FName QuestID, UDataTable* DataTable);

	UFUNCTION(BlueprintCallable, Category = "Quests")
	void TurnInQuest(FName QuestID);
};
