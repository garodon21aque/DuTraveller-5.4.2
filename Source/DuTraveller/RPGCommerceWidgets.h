#pragma once

#include "Blueprint/UserWidget.h"
#include "RPGVendor.h"
#include "RPG_UserWidget.h"
#include "RPGCommerceWidgets.generated.h"

class ARPGCharacter;
class UInventoryComponent;
class URPGCraftingComponent;
class UItemBlueprint;

/** Owning-client bridge for the retained crafting screen. Server authority stays in RPGCraftingComponent. */
UCLASS(BlueprintType, Blueprintable)
class DUTRAVELLER_API URPGCraftingMenuWidget : public URPG_UserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly, Category="Crafting") TObjectPtr<URPGCraftingComponent> SourceCrafting;
	UPROPERTY(BlueprintReadOnly, Category="Crafting") TObjectPtr<UInventoryComponent> PlayerInventory;
	UFUNCTION(BlueprintCallable, Category="Crafting") void InitializeCrafting(ARPGCharacter* Character);
	UFUNCTION(BlueprintPure, Category="Crafting") TArray<UItemBlueprint*> GetAvailableRecipes() const;
	UFUNCTION(BlueprintCallable, Category="Crafting") void CraftRecipe(UItemBlueprint* Recipe);
	UFUNCTION(BlueprintImplementableEvent, Category="Crafting") void RefreshCraftingMenu();
private:
	UFUNCTION() void HandleRecipesChanged();
	UFUNCTION() void HandleInventoryChanged();
};

/** Owning-client bridge for vendor offers and purchases. */
UCLASS(BlueprintType, Blueprintable)
class DUTRAVELLER_API URPGVendorMenuWidget : public URPG_UserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly, Category="Vendor") TObjectPtr<ARPGVendor> ActiveVendor;
	UPROPERTY(BlueprintReadOnly, Category="Vendor") TObjectPtr<UInventoryComponent> PlayerInventory;
	UFUNCTION(BlueprintCallable, Category="Vendor") void InitializeVendor(ARPGVendor* Vendor, ARPGCharacter* Character);
	UFUNCTION(BlueprintPure, Category="Vendor") TArray<FRPGVendorOffer> GetVendorOffers() const;
	UFUNCTION(BlueprintCallable, Category="Vendor") void PurchaseOffer(FName OfferId, int32 Count = 1);
	UFUNCTION(BlueprintImplementableEvent, Category="Vendor") void RefreshVendorMenu();
private:
	UFUNCTION() void HandleVendorChanged();
	UFUNCTION() void HandleInventoryChanged();
};
