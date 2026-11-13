#include "ItemObject.h"

#include "InventoryComponent.h"
#include "Consumable_ItemObject.h"
#include "Currency_ItemObject.h"
#include "Equipment_ItemObject.h"
#include "Food_ItemObject.h"
#include "Gear_ItemObject.h"
#include "ItemBlueprint.h"
#include "Potion_ItemObject.h"
#include "Net/UnrealNetwork.h"

UWorld* UItemObject::GetWorld() const
{
	return OwningInventory ? OwningInventory->GetWorld() : nullptr;
}

void UItemObject::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UItemObject, ItemId);
	DOREPLIFETIME(UItemObject, Quantity);
	DOREPLIFETIME(UItemObject, OwningInventory);
}

void UItemObject::OnRep_Quantity()
{
	OnItemModified.Broadcast();
	if (OwningInventory) OwningInventory->OnInventoryUpdated.Broadcast();
}

void UItemObject::OnRep_OwningInventory()
{
	if (OwningInventory) OwningInventory->OnInventoryUpdated.Broadcast();
}

bool UItemObject::ApplyUseEffect_Implementation(AActor* User)
{
	return false;
}

UItemObject::UItemObject()
{
	ItemDisplayName = NSLOCTEXT("DuTravellerItem", "UnnamedItem", "Unnamed Item");
	UseActionText = NSLOCTEXT("DuTravellerItem", "Use", "Use");
}

void UItemObject::Initialize(UInventoryComponent* NewOwningInventory, const int32 NewQuantity)
{
	if (!NewOwningInventory || !NewOwningInventory->GetOwner() || !NewOwningInventory->GetOwner()->HasAuthority()) return;
	OwningInventory = NewOwningInventory;
	if (!ItemId.IsValid()) ItemId = FGuid::NewGuid();
	Quantity = FMath::Max(0, NewQuantity);
}

void UItemObject::InitializePresentationPlaceholder(const int32 PresentationQuantity)
{
	OwningInventory = nullptr;
	ItemId.Invalidate();
	Quantity = FMath::Max(0, PresentationQuantity);
}

void UItemObject::SetQuantity(const int32 NewQuantity)
{
	if (!OwningInventory || !OwningInventory->GetOwner() || !OwningInventory->GetOwner()->HasAuthority()) return;
	const int32 ClampedQuantity = FMath::Max(0, NewQuantity);
	if (Quantity == ClampedQuantity)
	{
		return;
	}

	Quantity = ClampedQuantity;
	OnItemModified.Broadcast();
}

bool UItemObject::ShouldShowInInventory_Implementation() const
{
	return Quantity > 0;
}

FText UItemObject::GetResolvedDisplayName() const
{
	return ItemDisplayName.IsEmpty() ? GetClass()->GetDisplayNameText() : ItemDisplayName;
}

FText UItemObject::GetItemTypeText() const
{
	if (IsA<UGear_ItemObject>()) return NSLOCTEXT("RPGItems", "Gear", "Gear");
	if (IsA<UEquipment_ItemObject>()) return NSLOCTEXT("RPGItems", "Equipment", "Equipment");
	if (IsA<UFood_ItemObject>()) return NSLOCTEXT("RPGItems", "Food", "Food");
	if (IsA<UPotion_ItemObject>()) return NSLOCTEXT("RPGItems", "Potion", "Potion");
	if (IsA<UConsumable_ItemObject>()) return NSLOCTEXT("RPGItems", "Consumable", "Consumable");
	if (IsA<UCurrency_ItemObject>()) return NSLOCTEXT("RPGItems", "Currency", "Currency");
	if (IsA<UItemBlueprint>()) return NSLOCTEXT("RPGItems", "Recipe", "Recipe");
	return NSLOCTEXT("RPGItems", "Item", "Item");
}

FText UItemObject::GetRarityText() const
{
	const UEnum* Enum = StaticEnum<EItemRarity>();
	return Enum ? Enum->GetDisplayNameTextByValue(static_cast<int64>(Rarity)) : FText::GetEmpty();
}

FText UItemObject::GetTooltipDetailsText() const
{
	TArray<FString> Lines;
	if (const UEquipment_ItemObject* Equipment = Cast<UEquipment_ItemObject>(this))
	{
		if (const UEnum* Enum = StaticEnum<EEquipableSlot>())
		{
			Lines.Add(FString::Printf(TEXT("Slot: %s"),
				*Enum->GetDisplayNameTextByValue(static_cast<int64>(Equipment->Slot)).ToString()));
		}
		TArray<FName> Keys;
		Equipment->AttributeBonuses.GetKeys(Keys);
		Keys.Sort(FNameLexicalLess());
		for (const FName Key : Keys)
		{
			Lines.Add(FString::Printf(TEXT("%+.2f %s"),
				Equipment->AttributeBonuses.FindRef(Key), *FName::NameToDisplayString(Key.ToString(), false)));
		}
	}
	if (const UConsumable_ItemObject* Consumable = Cast<UConsumable_ItemObject>(this))
	{
		TArray<FName> Keys;
		Consumable->SetByCallerMagnitudes.GetKeys(Keys);
		Keys.Sort(FNameLexicalLess());
		for (const FName Key : Keys)
		{
			Lines.Add(FString::Printf(TEXT("%+.2f %s"),
				Consumable->SetByCallerMagnitudes.FindRef(Key), *Key.ToString()));
		}
	}
	if (const UItemBlueprint* Recipe = Cast<UItemBlueprint>(this))
	{
		if (Recipe->CraftedItem)
		{
			const UItemObject* Product = Recipe->CraftedItem->GetDefaultObject<UItemObject>();
			Lines.Add(FString::Printf(TEXT("Crafts: %s x%d"),
				*Product->GetResolvedDisplayName().ToString(), FMath::Max(1, Recipe->CraftedQuantity)));
		}
		for (const FRPGInventoryAmount& Ingredient : Recipe->Ingredients)
		{
			if (!Ingredient.ItemClass) continue;
			const UItemObject* Defaults = Ingredient.ItemClass->GetDefaultObject<UItemObject>();
			Lines.Add(FString::Printf(TEXT("Requires: %s x%d"),
				*Defaults->GetResolvedDisplayName().ToString(), FMath::Max(1, Ingredient.Quantity)));
		}
		if (Recipe->Ingredients.IsEmpty())
		{
			for (const TSubclassOf<UItemObject> IngredientClass : Recipe->RequiredItems)
			{
				if (!IngredientClass) continue;
				Lines.Add(FString::Printf(TEXT("Requires: %s x1"),
					*IngredientClass->GetDefaultObject<UItemObject>()->GetResolvedDisplayName().ToString()));
			}
		}
	}
	return FText::FromString(FString::Join(Lines, TEXT("\n")));
}
