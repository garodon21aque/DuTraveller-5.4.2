#include "InventoryComponent.h"
#include "Currency_ItemObject.h"
#include "Equipment_ItemObject.h"

#include "ItemObject.h"
#include "Net/UnrealNetwork.h"
#include "RPGCharacter.h"

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	bReplicateUsingRegisteredSubObjectList = true;
}

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UInventoryComponent, Capacity, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UInventoryComponent, Items, COND_OwnerOnly);
}

void UInventoryComponent::ReadyForReplication()
{
	Super::ReadyForReplication();
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		for (UItemObject* Item : Items)
		{
			if (!IsValid(Item)) continue;
			Item->Initialize(this, Item->GetQuantity());
			AddReplicatedSubObject(Item, COND_OwnerOnly);
		}
	}
}

void UInventoryComponent::OnRep_Items()
{
	NotifyInventoryUpdated();
}

UItemObject* UInventoryComponent::FindItemById(const FGuid ItemId) const
{
	for (UItemObject* Item : Items)
		if (IsValid(Item) && Item->ItemId == ItemId) return Item;
	return nullptr;
}

UItemObject* UInventoryComponent::FindItemByClass(const TSubclassOf<UItemObject> ItemClass)
{
	if (!ItemClass)
	{
		return nullptr;
	}

	for (UItemObject* Item : Items)
	{
		if (IsValid(Item) && Item->IsA(ItemClass))
		{
			return Item;
		}
	}
	if (ItemClass->IsChildOf(UCurrency_ItemObject::StaticClass()))
	{
		TObjectPtr<UItemObject>& EmptyDisplay = EmptyCurrencyPresentationItems.FindOrAdd(ItemClass);
		if (!EmptyDisplay)
		{
			EmptyDisplay = NewObject<UItemObject>(this, ItemClass);
			EmptyDisplay->InitializePresentationPlaceholder();
		}
		return EmptyDisplay;
	}
	return nullptr;
}

TArray<UItemObject*> UInventoryComponent::FindItemsByClass(const TSubclassOf<UItemObject> ItemClass) const
{
	TArray<UItemObject*> Result;
	if (!ItemClass)
	{
		return Result;
	}

	for (UItemObject* Item : Items)
	{
		if (IsValid(Item) && Item->IsA(ItemClass))
		{
			Result.Add(Item);
		}
	}
	return Result;
}

UItemObject* UInventoryComponent::FindItem(UItemObject* Item) const
{
	return Items.Contains(Item) ? Item : nullptr;
}

UItemObject* UInventoryComponent::GetItem(const int32 Index)
{
	if (Items.IsValidIndex(Index)) return Items[Index];
	if (Index < 0 || Index >= Capacity) return nullptr;
	if (!EmptySlotPresentationItem)
	{
		EmptySlotPresentationItem = NewObject<UItemObject>(this);
		EmptySlotPresentationItem->InitializePresentationPlaceholder();
	}
	return EmptySlotPresentationItem;
}

FItemAddResult UInventoryComponent::TryAddItemFromClass(const TSubclassOf<UItemObject> ItemClass, const int32 Quantity)
{
	FItemAddResult AddResult;
	AddResult.AmountToGive = FMath::Max(0, Quantity);
	if (!GetOwner() || !GetOwner()->HasAuthority() || bMutationInProgress) return AddResult;
	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	if (!ItemClass || Quantity <= 0)
	{
		AddResult.ErrorText = NSLOCTEXT("DuTravellerInventory", "InvalidItem", "The item or quantity is invalid.");
		return AddResult;
	}

	const UItemObject* ItemDefaults = ItemClass->GetDefaultObject<UItemObject>();
	int32 Remaining = Quantity;
	if (ItemDefaults->bStackable)
	{
		for (UItemObject* Stack : FindItemsByClass(ItemClass))
		{
			if (Stack->GetClass() != ItemClass) continue;
			const int32 Space = FMath::Max(0, Stack->MaxStackSize - Stack->GetQuantity());
			const int32 Added = FMath::Min(Space, Remaining);
			Stack->SetQuantity(Stack->GetQuantity() + Added);
			Remaining -= Added;
			if (Remaining == 0)
			{
				break;
			}
		}
	}

	while (Remaining > 0 && Items.Num() < Capacity)
	{
		const int32 StackQuantity = ItemDefaults->bStackable
			? FMath::Min(FMath::Max(1, ItemDefaults->MaxStackSize), Remaining)
			: 1;
		UItemObject* NewItem = NewObject<UItemObject>(this, ItemClass);
		NewItem->Initialize(this, StackQuantity);
		Items.Add(NewItem);
		if (IsReadyForReplication()) AddReplicatedSubObject(NewItem, COND_OwnerOnly);
		Remaining -= StackQuantity;
	}

	AddResult.AmountGiven = Quantity - Remaining;
	AddResult.Result = AddResult.AmountGiven == 0
		? EItemAddResult::IAR_NoItemsAdded
		: (Remaining == 0 ? EItemAddResult::IAR_AllItemsAdded : EItemAddResult::IAR_SomeItemsAdded);
	if (Remaining > 0)
	{
		AddResult.ErrorText = NSLOCTEXT("DuTravellerInventory", "InventoryFull", "The inventory is full.");
	}
	if (AddResult.AmountGiven > 0)
	{
		NotifyInventoryUpdated();
	}
	return AddResult;
}

bool UInventoryComponent::RemoveItem(UItemObject* Item)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bMutationInProgress) return false;
	// Worn equipment cannot be removed from the inventory directly; unequip it first.
	if (const ARPGCharacter* Character = Cast<ARPGCharacter>(GetOwner());
		Character && Character->IsItemEquipped(Item)) return false;
	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	return RemoveItemInternal(Item);
}

bool UInventoryComponent::RemoveItemInternal(UItemObject* Item)
{
	if (!IsValid(Item) || !Items.RemoveSingle(Item))
	{
		return false;
	}
	if (IsReadyForReplication()) RemoveReplicatedSubObject(Item);
	Item->OwningInventory = nullptr;
	NotifyInventoryUpdated();
	return true;
}

bool UInventoryComponent::UseItem(UItemObject* Item)
{
	if (!GetOwner() || bMutationInProgress) return false;
	if (!GetOwner()->HasAuthority())
	{
		ServerUseItem(Item);
		return false; // Completion is reported by replicated inventory updates.
	}
	if (!IsValid(Item) || !Items.Contains(Item) || Item->GetQuantity() <= 0)
	{
		return false;
	}
	if (UEquipment_ItemObject* Equipment = Cast<UEquipment_ItemObject>(Item))
	{
		ARPGCharacter* Character = Cast<ARPGCharacter>(GetOwner());
		return Character && (Character->IsItemEquipped(Item)
			? Character->TryUnequipItem(Equipment->Slot) : Character->TryEquipItem(Equipment, Equipment->Slot));
	}
	TGuardValue<bool> MutationGuard(bMutationInProgress, true);

	if (!Item->ApplyUseEffect(GetOwner())) return false;
	return ConsumeItemInternal(Item, 1);
}

void UInventoryComponent::ServerUseItem_Implementation(UItemObject* Item)
{
	UseItem(Item);
}

bool UInventoryComponent::ConsumeItem(UItemObject* Item, const int32 Quantity)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bMutationInProgress || !IsValid(Item) ||
		!Items.Contains(Item) || Item->OwningInventory != this || Quantity <= 0 || Item->GetQuantity() < Quantity) return false;
	if (const ARPGCharacter* Character = Cast<ARPGCharacter>(GetOwner()); Character && Character->IsItemEquipped(Item)) return false;
	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	return ConsumeItemInternal(Item, Quantity);
}

bool UInventoryComponent::ConsumeItemInternal(UItemObject* Item, const int32 Quantity)
{
	if (!IsValid(Item) || !Items.Contains(Item) || Item->OwningInventory != this || Quantity <= 0 || Item->GetQuantity() < Quantity) return false;
	Item->SetQuantity(Item->GetQuantity() - Quantity);
	if (Item->GetQuantity() == 0) return RemoveItemInternal(Item);
	NotifyInventoryUpdated();
	return true;
}

int64 UInventoryComponent::CountItemsOfExactClass(const TSubclassOf<UItemObject> ItemClass) const
{
	int64 Result = 0;
	for (const UItemObject* Item : Items)
		if (IsValid(Item) && Item->GetClass() == ItemClass) Result += FMath::Max(0, Item->GetQuantity());
	return Result;
}

bool UInventoryComponent::TryExchangeItems(const TArray<FRPGInventoryAmount>& Costs, const TArray<FRPGInventoryAmount>& Outputs)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bMutationInProgress) return false;
	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	struct FPlannedStack { UItemObject* Item; TSubclassOf<UItemObject> Class; int32 Quantity; };
	TArray<FPlannedStack> Plan;
	const ARPGCharacter* Character = Cast<ARPGCharacter>(GetOwner());
	for (UItemObject* Item : Items)
	{
		if (!IsValid(Item) || Item->OwningInventory != this || Item->GetQuantity() <= 0) return false;
		Plan.Add({Item, Item->GetClass(), Item->GetQuantity()});
	}
	for (const FRPGInventoryAmount& Cost : Costs)
	{
		if (!Cost.ItemClass || Cost.Quantity <= 0) return false;
		int32 Remaining = Cost.Quantity;
		for (FPlannedStack& Stack : Plan)
		{
			if (Stack.Class != Cost.ItemClass || (Character && Character->IsItemEquipped(Stack.Item))) continue;
			const int32 Taken = FMath::Min(Remaining, Stack.Quantity);
			Stack.Quantity -= Taken; Remaining -= Taken;
			if (Remaining == 0) break;
		}
		if (Remaining != 0) return false;
	}
	for (const FRPGInventoryAmount& Output : Outputs)
	{
		if (!Output.ItemClass || Output.Quantity <= 0) return false;
		const UItemObject* Defaults = Output.ItemClass->GetDefaultObject<UItemObject>();
		int32 Remaining = Output.Quantity;
		if (Defaults->bStackable)
		{
			for (FPlannedStack& Stack : Plan)
			{
				if (Stack.Class != Output.ItemClass) continue;
				const int32 Added = FMath::Min(Remaining, FMath::Max(0, Defaults->MaxStackSize - Stack.Quantity));
				Stack.Quantity += Added; Remaining -= Added;
				if (Remaining == 0) break;
			}
		}
		while (Remaining > 0)
		{
			int32 Occupied = 0;
			for (const FPlannedStack& Stack : Plan) if (Stack.Quantity > 0) ++Occupied;
			if (Occupied >= Capacity) return false;
			const int32 Added = Defaults->bStackable ? FMath::Min(FMath::Max(1, Defaults->MaxStackSize), Remaining) : 1;
			Plan.Add({nullptr, Output.ItemClass, Added});
			Remaining -= Added;
		}
	}
	TArray<TObjectPtr<UItemObject>> Result;
	for (FPlannedStack& Stack : Plan)
	{
		if (Stack.Quantity <= 0)
		{
			if (Stack.Item)
			{
				Stack.Item->SetQuantity(0);
				if (IsReadyForReplication()) RemoveReplicatedSubObject(Stack.Item);
				Stack.Item->OwningInventory = nullptr;
			}
			continue;
		}
		if (!Stack.Item)
		{
			Stack.Item = NewObject<UItemObject>(this, Stack.Class);
			Stack.Item->Initialize(this, Stack.Quantity);
			if (IsReadyForReplication()) AddReplicatedSubObject(Stack.Item, COND_OwnerOnly);
		}
		else Stack.Item->SetQuantity(Stack.Quantity);
		Result.Add(Stack.Item);
	}
	Items = MoveTemp(Result);
	NotifyInventoryUpdated();
	return true;
}

void UInventoryComponent::NotifyInventoryUpdated()
{
	OnInventoryUpdated.Broadcast();
}
