#include "DTEquipmentWidget.h"
#include "DTInventoryDragDrop.h"
#include "RPGCharacter.h"
#include "InventoryComponent.h"
#include "Equipment_ItemObject.h"
#include "RPG_ItemToolTip.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "UObject/UnrealType.h"

void UDTEquipmentSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshEquipmentSlot();
}

void UDTEquipmentSlotWidget::RefreshEquipmentSlot()
{
	// Preserve each existing designer instance's historical ItemSlot setting.
	if (const FByteProperty* Legacy = FindFProperty<FByteProperty>(GetClass(), TEXT("ItemSlot")))
		EquipmentSlot = static_cast<EEquipableSlot>(Legacy->GetPropertyValue_InContainer(this));
	else if (const FEnumProperty* LegacyEnum = FindFProperty<FEnumProperty>(GetClass(), TEXT("ItemSlot")))
		EquipmentSlot = static_cast<EEquipableSlot>(LegacyEnum->GetUnderlyingProperty()->GetSignedIntPropertyValue(LegacyEnum->ContainerPtrToValuePtr<void>(this)));
	ARPGCharacter* Character = Cast<ARPGCharacter>(GetOwningPlayerPawn());
	EquippedItem = Character ? Character->GetEquippedItems().FindRef(EquipmentSlot) : nullptr;
	if (FObjectPropertyBase* Legacy = FindFProperty<FObjectPropertyBase>(GetClass(), TEXT("Item")))
		Legacy->SetObjectPropertyValue_InContainer(this, EquippedItem);
	if (UImage* Image = Cast<UImage>(GetWidgetFromName(TEXT("ItemThumbnail"))))
	{
		Image->SetBrushFromTexture(EquippedItem ? EquippedItem->Thumbnail.Get() : nullptr);
		Image->SetVisibility(EquippedItem ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (EquippedItem && GetOwningPlayer())
	{
		if (!EquipmentTooltip)
		{
			UClass* Type = LoadClass<URPG_ItemToolTip>(nullptr, TEXT("/Game/UI/InventorySystem/WBP_InvItemTooltip.WBP_InvItemTooltip_C"));
			if (Type) EquipmentTooltip = CreateWidget<URPG_ItemToolTip>(GetOwningPlayer(), Type);
		}
		if (EquipmentTooltip) { EquipmentTooltip->SetItem(EquippedItem); SetToolTip(EquipmentTooltip); }
	}
	else { EquipmentTooltip = nullptr; SetToolTip(nullptr); }
}

FReply UDTEquipmentSlotWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (EquippedItem && Event.GetEffectingButton() == EKeys::LeftMouseButton)
		return UWidgetBlueprintLibrary::DetectDragIfPressed(Event, this, EKeys::LeftMouseButton).NativeReply;
	if (EquippedItem && Event.GetEffectingButton() == EKeys::RightMouseButton)
	{
		if (ARPGCharacter* Character = Cast<ARPGCharacter>(GetOwningPlayerPawn())) Character->UnequipItem(EquipmentSlot);
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(Geometry, Event);
}

void UDTEquipmentSlotWidget::NativeOnDragDetected(const FGeometry& Geometry, const FPointerEvent& Event, UDragDropOperation*& Operation)
{
	Operation = UDTInventoryDragDrop::BeginItemDrag(this, EquippedItem, true);
}

bool UDTEquipmentSlotWidget::NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Operation)
{
	if (UDTInventoryDragDrop::ResolveItem(Operation))
	{
		if (EquipmentSlot != EEquipableSlot::EIS_None) UDTInventoryDragDrop::ApplyEquipmentDrop(this, Operation, EquipmentSlot);
		return true; // A rejected slot drop must not bubble into an unequip action.
	}
	return Super::NativeOnDrop(Geometry, Event, Operation);
}

void UDTEquipmentWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BoundCharacter = Cast<ARPGCharacter>(GetOwningPlayerPawn());
	if (BoundCharacter)
	{
		BoundCharacter->OnEquipedItemsChanged.AddUniqueDynamic(this, &ThisClass::RefreshEquipmentPanel);
		BoundCharacter->InventoryComp->OnInventoryUpdated.AddUniqueDynamic(this, &ThisClass::RefreshEquipmentPanel);
	}
	RefreshEquipmentPanel();
}

void UDTEquipmentWidget::NativeDestruct()
{
	if (BoundCharacter)
	{
		BoundCharacter->OnEquipedItemsChanged.RemoveAll(this);
		BoundCharacter->InventoryComp->OnInventoryUpdated.RemoveAll(this);
	}
	BoundCharacter = nullptr;
	Super::NativeDestruct();
}

void UDTEquipmentWidget::RefreshEquipmentPanel()
{
	if (WidgetTree) WidgetTree->ForEachWidget([](UWidget* Widget)
	{
		if (UDTEquipmentSlotWidget* Equipment = Cast<UDTEquipmentSlotWidget>(Widget)) Equipment->RefreshEquipmentSlot();
	});
}

bool UDTEquipmentWidget::NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Operation)
{
	if (UDTInventoryDragDrop::ResolveItem(Operation))
	{
		UDTInventoryDragDrop::ApplyEquipmentDrop(this, Operation);
		return true;
	}
	return Super::NativeOnDrop(Geometry, Event, Operation);
}
