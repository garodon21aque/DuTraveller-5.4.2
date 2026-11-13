#include "RPGCommerceWidgets.h"

#include "InventoryComponent.h"
#include "ItemBlueprint.h"
#include "RPGCharacter.h"
#include "RPGCraftingComponent.h"

void URPGCraftingMenuWidget::InitializeCrafting(ARPGCharacter* Character)
{
	if (!IsValid(Character) || !Character->IsLocallyControlled()) return;
	if (SourceCrafting) SourceCrafting->OnRecipesChanged.RemoveAll(this);
	if (PlayerInventory) PlayerInventory->OnInventoryUpdated.RemoveAll(this);
	SourceCrafting = Character->CraftingComp;
	PlayerInventory = Character->InventoryComp;
	if (SourceCrafting) SourceCrafting->OnRecipesChanged.AddDynamic(this, &ThisClass::HandleRecipesChanged);
	if (PlayerInventory) PlayerInventory->OnInventoryUpdated.AddDynamic(this, &ThisClass::HandleInventoryChanged);
	RefreshCraftingMenu();
}

TArray<UItemBlueprint*> URPGCraftingMenuWidget::GetAvailableRecipes() const
{
	return SourceCrafting ? SourceCrafting->GetRecipes() : TArray<UItemBlueprint*>();
}

void URPGCraftingMenuWidget::CraftRecipe(UItemBlueprint* Recipe)
{
	if (SourceCrafting) SourceCrafting->Craft(Recipe);
}

void URPGCraftingMenuWidget::HandleRecipesChanged() { RefreshCraftingMenu(); }
void URPGCraftingMenuWidget::HandleInventoryChanged() { RefreshCraftingMenu(); }

void URPGVendorMenuWidget::InitializeVendor(ARPGVendor* Vendor, ARPGCharacter* Character)
{
	if (!IsValid(Vendor) || !IsValid(Character) || !Character->IsLocallyControlled()) return;
	if (ActiveVendor) ActiveVendor->OnVendorChanged.RemoveAll(this);
	if (PlayerInventory) PlayerInventory->OnInventoryUpdated.RemoveAll(this);
	ActiveVendor = Vendor;
	PlayerInventory = Character->InventoryComp;
	ActiveVendor->OnVendorChanged.AddDynamic(this, &ThisClass::HandleVendorChanged);
	if (PlayerInventory) PlayerInventory->OnInventoryUpdated.AddDynamic(this, &ThisClass::HandleInventoryChanged);
	RefreshVendorMenu();
}

TArray<FRPGVendorOffer> URPGVendorMenuWidget::GetVendorOffers() const
{
	return ActiveVendor ? ActiveVendor->Offers : TArray<FRPGVendorOffer>();
}

void URPGVendorMenuWidget::PurchaseOffer(const FName OfferId, const int32 Count)
{
	ARPGCharacter* Character = GetOwningPlayerPawn<ARPGCharacter>();
	if (Character && ActiveVendor) Character->PurchaseFromVendor(ActiveVendor, OfferId, Count);
}

void URPGVendorMenuWidget::HandleVendorChanged() { RefreshVendorMenu(); }
void URPGVendorMenuWidget::HandleInventoryChanged() { RefreshVendorMenu(); }
