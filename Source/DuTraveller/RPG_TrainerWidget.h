#pragma once

#include "RPG_UserWidget.h"
#include "RPG_TrainerWidget.generated.h"

class URPGAction;
class URPGActionComponent;

/** Legacy trainer widget base; receives the trained actor's action list on spawn. */
UCLASS(Blueprintable, BlueprintType)
class DUTRAVELLER_API URPG_TrainerWidget : public URPG_UserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Instanced, meta = (ExposeOnSpawn = "true"), Category = "Trainer")
	TArray<TObjectPtr<URPGAction>> Actions;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = "true"), Category = "Trainer")
	TObjectPtr<URPGActionComponent> ActionComp;
};
