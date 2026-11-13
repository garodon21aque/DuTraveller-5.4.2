#pragma once

#include "ItemObject.h"
#include "Currency_ItemObject.generated.h"

/** Currency remains an inventory item, preserving the original gold/bronze UI model. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class DUTRAVELLER_API UCurrency_ItemObject : public UItemObject
{
	GENERATED_BODY()
public:
	UCurrency_ItemObject()
	{
		bStackable = true;
		MaxStackSize = MAX_int32;
	}
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Currency")
	FName CurrencyId;
};
