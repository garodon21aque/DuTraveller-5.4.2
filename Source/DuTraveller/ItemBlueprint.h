#pragma once

#include "ItemObject.h"
#include "InventoryComponent.h"
#include "ItemBlueprint.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRecipeModifiedSignature);

/**
 * Legacy crafting recipe item ("Item Blueprint"). Crafting widgets hold a
 * `Recipe` reference of this type and bind its `OnRecipeModified` dispatcher.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class DUTRAVELLER_API UItemBlueprint : public UItemObject
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintAssignable, Category = "Recipe")
	FOnRecipeModifiedSignature OnRecipeModified;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Recipe")
	TArray<TSubclassOf<UItemObject>> RequiredItems;

	/** Explicit quantities. If empty, each legacy RequiredItems entry costs one item. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Recipe")
	TArray<FRPGInventoryAmount> Ingredients;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Recipe")
	TSubclassOf<UItemObject> CraftedItem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Recipe")
	int32 CraftedQuantity = 1;
};
