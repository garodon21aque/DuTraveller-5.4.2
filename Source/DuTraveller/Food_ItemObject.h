#pragma once

#include "Consumable_ItemObject.h"
#include "Food_ItemObject.generated.h"

/** Consumable item specialization retained by inventory filtering graphs. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class DUTRAVELLER_API UFood_ItemObject : public UConsumable_ItemObject
{
	GENERATED_BODY()
};
