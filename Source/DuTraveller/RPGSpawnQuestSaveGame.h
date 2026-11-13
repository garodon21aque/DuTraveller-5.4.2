#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "RPGDynamicQuestTypes.h"
#include "RPGSpawnArea.h"
#include "RPGSpawnQuestSaveGame.generated.h"

/** Snapshot container for the server save flow. Capture/restore APIs never grant rewards. */
UCLASS(BlueprintType)
class DUTRAVELLER_API URPGSpawnQuestSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FRPGDynamicQuestSnapshot PlayerQuests;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FRPGSpawnAreaSnapshot> SpawnAreas;
};
