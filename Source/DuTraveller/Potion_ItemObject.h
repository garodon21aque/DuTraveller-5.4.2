#pragma once

#include "Consumable_ItemObject.h"
#include "Potion_ItemObject.generated.h"

/** Potion branch; effects and strength are item data rather than fixed class restrictions. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class DUTRAVELLER_API UPotion_ItemObject : public UConsumable_ItemObject
{
	GENERATED_BODY()
};
