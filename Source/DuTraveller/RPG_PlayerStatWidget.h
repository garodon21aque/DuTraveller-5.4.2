#pragma once

#include "RPG_UserWidget.h"
#include "RPG_PlayerStatWidget.generated.h"

class URPGAttributeComponent;

/** Legacy player-stat widget base; exposes the cached stat values the panel binds. */
UCLASS(Blueprintable, BlueprintType)
class DUTRAVELLER_API URPG_PlayerStatWidget : public URPG_UserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = "true"), Category = "Stats")
	TObjectPtr<URPGAttributeComponent> AttributeComp;
};
