#include "DTInventoryDragDrop.h"
#include "Blueprint/UserWidget.h"
#include "Equipment_ItemObject.h"
#include "InventoryComponent.h"
#include "RPGCharacter.h"
#include "UObject/UnrealType.h"
#include "DTPlayerInventoryWidget.h"
#include "DTEquipmentWidget.h"
#include "RPG_ItemWidget.h"
#include "RPGPlayerController.h"
#include "Components/WrapBox.h"
#include "Components/Image.h"
#include "Gear_ItemObject.h"

UItemObject* UDTInventoryDragDrop::CreateVerificationGear(APlayerController* Controller, const FText DisplayName,
	const EEquipableSlot ItemSlot, const TMap<FName, float>& Bonuses)
{
#if WITH_EDITOR
	ARPGCharacter* Character = Controller ? Cast<ARPGCharacter>(Controller->GetPawn()) : nullptr;
	if (!Character || !Character->HasAuthority() || !Character->GetWorld()->IsGameWorld()) return nullptr;
	TSet<UItemObject*> Before;
	for (UItemObject* Item : Character->InventoryComp->Items) Before.Add(Item);
	if (Character->InventoryComp->TryAddItemFromClass(UGear_ItemObject::StaticClass(), 1).AmountGiven != 1) return nullptr;
	for (UItemObject* Item : Character->InventoryComp->Items)
		if (!Before.Contains(Item))
		{
			UEquipment_ItemObject* Gear = CastChecked<UEquipment_ItemObject>(Item);
			Gear->ItemDisplayName = DisplayName;
			Gear->Slot = ItemSlot;
			Gear->AttributeBonuses = Bonuses;
			for (const auto& Pair : Character->GetEquippedItems())
				if (Pair.Value && Pair.Value->Thumbnail) { Gear->Thumbnail = Pair.Value->Thumbnail; break; }
			Gear->OnItemModified.Broadcast();
			Character->InventoryComp->OnInventoryUpdated.Broadcast();
			return Gear;
		}
#endif
	return nullptr;
}

UItemObject* UDTInventoryDragDrop::ResolveItem(UDragDropOperation* Operation)
{
	if (!Operation) return nullptr;
	if (const UDTInventoryDragDrop* Native = Cast<UDTInventoryDragDrop>(Operation)) return Native->DraggedItem;
	if (UItemObject* Item = Cast<UItemObject>(Operation->Payload)) return Item;
	if (UObject* Payload = Operation->Payload)
		if (const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Payload->GetClass(), TEXT("Item")))
			return Cast<UItemObject>(Property->GetObjectPropertyValue_InContainer(Payload));
	return nullptr;
}

UDTInventoryDragDrop* UDTInventoryDragDrop::BeginItemDrag(UUserWidget* Source, UItemObject* Item, const bool bEquipped)
{
	ARPGCharacter* Character = Source ? Cast<ARPGCharacter>(Source->GetOwningPlayerPawn()) : nullptr;
	if (!Character || !Item || Item->GetQuantity() <= 0 || Item->OwningInventory != Character->InventoryComp ||
		Character->InventoryComp->FindItem(Item) != Item) return nullptr;
	const TCHAR* OperationPath = bEquipped ?
		TEXT("/Game/UI/InventorySystem/DragDrop/BP_EquipmentDragDropOperation.BP_EquipmentDragDropOperation_C") :
		TEXT("/Game/UI/InventorySystem/DragDrop/BP_ItemDragDropOperation.BP_ItemDragDropOperation_C");
	UClass* OperationClass = LoadClass<UDTInventoryDragDrop>(nullptr, OperationPath);
	UDTInventoryDragDrop* Operation = NewObject<UDTInventoryDragDrop>(GetTransientPackage(), OperationClass ? OperationClass : StaticClass());
	Operation->DraggedItem = Item;
	Operation->Payload = Item;
	const TCHAR* PayloadPath = bEquipped ? TEXT("/Game/UI/InventorySystem/DragDrop/BP_EquipmentDragDrop.BP_EquipmentDragDrop_C") :
		TEXT("/Game/UI/InventorySystem/DragDrop/BP_ItemDragDrop.BP_ItemDragDrop_C");
	if (UClass* PayloadClass = LoadClass<UObject>(nullptr, PayloadPath))
	{
		UObject* Payload = NewObject<UObject>(Operation, PayloadClass);
		if (FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(PayloadClass, TEXT("Item")))
			if (Item->IsA(Property->PropertyClass)) Property->SetObjectPropertyValue_InContainer(Payload, Item);
		for (const FName Name : {FName("InvItemWidget"), FName("ItemWidget"), FName("EquippedItemWidget")})
			if (FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(PayloadClass, Name))
				if (Source->IsA(Property->PropertyClass)) Property->SetObjectPropertyValue_InContainer(Payload, Source);
		Operation->Payload = Payload;
	}
	Operation->bFromEquipment = bEquipped;
	Operation->Pivot = EDragPivot::MouseDown;
	UClass* VisualClass = LoadClass<UUserWidget>(nullptr,
		TEXT("/Game/UI/InventorySystem/DragDrop/WBP_ItemDragVisual.WBP_ItemDragVisual_C"));
	if (VisualClass && Source->GetOwningPlayer())
	{
		UUserWidget* Visual = CreateWidget<UUserWidget>(Source->GetOwningPlayer(), VisualClass);
		if (Visual)
		{
			if (FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(VisualClass, TEXT("Item")))
				if (Item->IsA(Property->PropertyClass)) Property->SetObjectPropertyValue_InContainer(Visual, Item);
			Visual->SetVisibility(ESlateVisibility::HitTestInvisible);
			Operation->DefaultDragVisual = Visual;
		}
	}
	return Operation;
}

bool UDTInventoryDragDrop::ApplyEquipmentDrop(UUserWidget* Target, UDragDropOperation* Operation,
	const EEquipableSlot RequiredSlot, const bool bUnequip)
{
	ARPGCharacter* Character = Target ? Cast<ARPGCharacter>(Target->GetOwningPlayerPawn()) : nullptr;
	UEquipment_ItemObject* Item = Cast<UEquipment_ItemObject>(ResolveItem(Operation));
	if (!Character || !Item || Item->GetQuantity() <= 0 || Item->OwningInventory != Character->InventoryComp ||
		Character->InventoryComp->FindItem(Item) != Item || Item->Slot == EEquipableSlot::EIS_None ||
		(RequiredSlot != EEquipableSlot::EIS_None && Item->Slot != RequiredSlot)) return false;
	if (bUnequip)
	{
		if (!Character->IsItemEquipped(Item)) return false;
		if (Character->HasAuthority()) return Character->TryUnequipItem(Item->Slot);
		Character->UnequipItem(Item->Slot);
	}
	else
	{
		if (Character->IsItemEquipped(Item)) return true;
		if (Character->HasAuthority()) return Character->TryEquipItem(Item, Item->Slot);
		Character->EquipItem(Item, Item->Slot);
	}
	// Clients wait for authoritative equipment/attribute replication to update UI.
	return true;
}

bool UDTInventoryDragDrop::DriveInventoryDrop(APlayerController* Controller, UItemObject* Item, const FName TargetName)
{
#if WITH_EDITOR
	ARPGPlayerController* PC = Cast<ARPGPlayerController>(Controller);
	UDTPlayerInventoryWidget* Inventory = PC ? Cast<UDTPlayerInventoryWidget>(PC->Panels.FindRef(TEXT("Inventory"))) : nullptr;
	ARPGCharacter* Character = PC ? Cast<ARPGCharacter>(PC->GetPawn()) : nullptr;
	if (!Inventory || !Character || !Item) return false;
	UDTEquipmentWidget* Equipment = Cast<UDTEquipmentWidget>(Inventory->GetWidgetFromName(TEXT("WBP_Equipment")));
	UWrapBox* Grid = Cast<UWrapBox>(Inventory->GetWidgetFromName(TEXT("InventoryWB")));
	UUserWidget* Source = nullptr;
	if (Character->IsItemEquipped(Item) && Equipment)
	{
		Equipment->RefreshEquipmentPanel();
		for (const FName Name : {FName("HelmetSlot"), FName("ChestSlot"), FName("LegsSlot"), FName("BootSlot"), FName("GlovesSlot"), FName("WeaponSlot"), FName("OffhandSlot"), FName("BackPackSlot"), FName("GrimoireSlot")})
			if (UDTEquipmentSlotWidget* SlotWidget = Cast<UDTEquipmentSlotWidget>(Equipment->GetWidgetFromName(Name)))
				if (SlotWidget->EquippedItem == Item) { Source = SlotWidget; break; }
	}
	else if (Grid)
		for (UWidget* Child : Grid->GetAllChildren())
			if (URPG_ItemWidget* ItemWidget = Cast<URPG_ItemWidget>(Child))
				if (ItemWidget->Item == Item) { Source = ItemWidget; break; }
	if (!Source) return false;
	UWidget* Destination = TargetName == TEXT("Character") ? Inventory->CharacterPreview.Get() :
		TargetName == TEXT("Inventory") ? static_cast<UWidget*>(Grid) : Equipment ? Equipment->GetWidgetFromName(TargetName) : nullptr;
	if (!Destination) return false;
	const FVector2D Position = Destination->GetCachedGeometry().LocalToAbsolute(Destination->GetCachedGeometry().GetLocalSize() * 0.5f);
	const FPointerEvent Pointer(0, Position, Position, TSet<FKey>{EKeys::LeftMouseButton}, EKeys::LeftMouseButton, 0.f, FModifierKeysState());
	UDragDropOperation* Operation = nullptr;
	if (URPG_ItemWidget* ItemWidget = Cast<URPG_ItemWidget>(Source))
	{
		if (!ItemWidget->NativeOnMouseButtonDown(Source->GetCachedGeometry(), Pointer).IsEventHandled()) return false;
		ItemWidget->NativeOnDragDetected(Source->GetCachedGeometry(), Pointer, Operation);
	}
	else if (UDTEquipmentSlotWidget* SlotWidget = Cast<UDTEquipmentSlotWidget>(Source))
	{
		if (!SlotWidget->NativeOnMouseButtonDown(Source->GetCachedGeometry(), Pointer).IsEventHandled()) return false;
		SlotWidget->NativeOnDragDetected(Source->GetCachedGeometry(), Pointer, Operation);
	}
	if (!Operation) return false;
	const FDragDropEvent Drop(Pointer, TSharedPtr<FDragDropOperation>());
	if (UDTEquipmentSlotWidget* SlotWidget = Cast<UDTEquipmentSlotWidget>(Destination))
		return SlotWidget->NativeOnDrop(Destination->GetCachedGeometry(), Drop, Operation);
	return Inventory->NativeOnDrop(Inventory->GetCachedGeometry(), Drop, Operation);
#else
	return false;
#endif
}
