#pragma once

#include "RPGNPCCharacter.h"
#include "RPGQuestGiver.generated.h"

/** Legacy quest-giver actor; parent of the missing BP_QuestGiver Blueprint class.
 *  Its serialized member variables (DataTable, QuestID, ...) were declared on the
 *  Blueprint asset and are restored by Tools/Restore-BPMemberVariables.py. */
UCLASS(BlueprintType, Blueprintable)
class DUTRAVELLER_API ARPGQuestGiver : public ARPGNPCCharacter
{
	GENERATED_BODY()

public:
	ARPGQuestGiver();

	UFUNCTION(BlueprintCallable, Category = "Quest")
	void IsAllObjectiveComplete(bool& Complete);
};
