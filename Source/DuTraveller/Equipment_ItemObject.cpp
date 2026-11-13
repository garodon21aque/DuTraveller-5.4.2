#include "Equipment_ItemObject.h"
#include "InventoryComponent.h"
#include "RPGCharacter.h"

bool UEquipment_ItemObject::IsEquipped() const
{
	const ARPGCharacter* Character = OwningInventory ? Cast<ARPGCharacter>(OwningInventory->GetOwner()) : nullptr;
	return Character && Character->IsItemEquipped(this);
}

bool UEquipment_ItemObject::ShouldShowInInventory_Implementation() const
{
	return Super::ShouldShowInInventory_Implementation() && !IsEquipped();
}
