#pragma once

#include "Equipment_ItemObject.h"
#include "Gear_ItemObject.generated.h"

/** Wearable/wieldable gear: weapons, armour, clothing and trinkets share this branch. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class DUTRAVELLER_API UGear_ItemObject : public UEquipment_ItemObject
{
	GENERATED_BODY()
};
