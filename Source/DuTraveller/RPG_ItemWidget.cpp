#include "RPG_ItemWidget.h"

#include "ItemObject.h"
#include "RPG_ItemToolTip.h"
#include "UObject/UnrealType.h"
#include "DTInventoryDragDrop.h"
#include "InventoryComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

void URPG_ItemWidget::EnsurePresentationItem()
{
	FObjectPropertyBase* LegacyItem = FindFProperty<FObjectPropertyBase>(GetClass(), TEXT("ItemObject"));
	if (!Item && LegacyItem)
	{
		Item = Cast<UItemObject>(LegacyItem->GetObjectPropertyValue_InContainer(this));
	}
	if (!Item)
	{
		PresentationPlaceholder = NewObject<UItemObject>(this);
		PresentationPlaceholder->InitializePresentationPlaceholder();
		Item = PresentationPlaceholder;
	}
	// Retained Blueprints read their historical ItemObject member during Construct.
	// Keep it synchronized even when the modern exposed Item input was supplied.
	if (LegacyItem)
	{
		LegacyItem->SetObjectPropertyValue_InContainer(this, Item);
	}
}

void URPG_ItemWidget::NativePreConstruct()
{
	EnsurePresentationItem();
	Super::NativePreConstruct();
}

void URPG_ItemWidget::NativeConstruct()
{
	EnsurePresentationItem();
	Super::NativeConstruct();
	RefreshItemPresentation();
}

void URPG_ItemWidget::NativeDestruct()
{
	if (Item)
	{
		Item->OnItemModified.RemoveDynamic(this, &URPG_ItemWidget::HandleItemModified);
	}
	RuntimeTooltip = nullptr;
	PresentationPlaceholder = nullptr;
	SetToolTip(nullptr);
	Super::NativeDestruct();
}

void URPG_ItemWidget::SetItem(UItemObject* NewItem)
{
	if (Item && Item != NewItem)
	{
		Item->OnItemModified.RemoveDynamic(this, &URPG_ItemWidget::HandleItemModified);
	}
	Item = NewItem;
	if (!Item)
	{
		if (!PresentationPlaceholder)
		{
			PresentationPlaceholder = NewObject<UItemObject>(this);
			PresentationPlaceholder->InitializePresentationPlaceholder();
		}
		Item = PresentationPlaceholder;
	}
	if (FObjectPropertyBase* LegacyItem = FindFProperty<FObjectPropertyBase>(GetClass(), TEXT("ItemObject")))
	{
		LegacyItem->SetObjectPropertyValue_InContainer(this, Item);
	}
	RefreshItemPresentation();
}

void URPG_ItemWidget::RefreshItemPresentation()
{
	if (!Item)
	{
		RuntimeTooltip = nullptr;
		SetToolTip(nullptr);
		return;
	}

	Item->OnItemModified.RemoveDynamic(this, &URPG_ItemWidget::HandleItemModified);
	Item->OnItemModified.AddUniqueDynamic(this, &URPG_ItemWidget::HandleItemModified);
	ItemDisplayName = Item->GetResolvedDisplayName();
	UpdateItemVisuals();
	if (Item->GetQuantity() <= 0)
	{
		RuntimeTooltip = nullptr;
		SetToolTip(nullptr);
		return;
	}
	if (Item->ItemTooltip)
	{
		ItemTooltip = Item->ItemTooltip;
	}
	if (!ItemTooltip)
	{
		ItemTooltip = LoadClass<URPG_ItemToolTip>(nullptr,
			TEXT("/Game/UI/InventorySystem/WBP_InvItemTooltip.WBP_InvItemTooltip_C"));
	}
	if (!ItemTooltip || !GetOwningPlayer())
	{
		RuntimeTooltip = nullptr;
		SetToolTip(nullptr);
		return;
	}

	RuntimeTooltip = CreateWidget<URPG_ItemToolTip>(GetOwningPlayer(), ItemTooltip);
	if (RuntimeTooltip)
	{
		RuntimeTooltip->InvItemWidget = this;
		RuntimeTooltip->SetItem(Item);
		SetToolTip(RuntimeTooltip);
	}
}

void URPG_ItemWidget::HandleItemModified()
{
	UpdateItemVisuals();
	if (RuntimeTooltip)
	{
		RuntimeTooltip->RefreshFromItem();
	}
}

void URPG_ItemWidget::UpdateItemVisuals()
{
	if (!Item) return;
	const bool bHasItem = Item->GetQuantity() > 0;
	if (UTextBlock* Name = Cast<UTextBlock>(GetWidgetFromName(TEXT("ItemName"))))
		Name->SetText(bHasItem ? Item->GetResolvedDisplayName() : FText::GetEmpty());
	if (UTextBlock* Quantity = Cast<UTextBlock>(GetWidgetFromName(TEXT("QuantityTXT"))))
		Quantity->SetText(bHasItem && Item->bStackable ? FText::AsNumber(Item->GetQuantity()) : FText::GetEmpty());
	if (UImage* Image = Cast<UImage>(GetWidgetFromName(TEXT("Thumbnail"))))
	{
		Image->SetBrushFromTexture(bHasItem ? Item->Thumbnail.Get() : nullptr);
		Image->SetVisibility(bHasItem ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

FReply URPG_ItemWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (bHandleInventoryInput && Item && Item->OwningInventory && Item->GetQuantity() > 0)
	{
		if (Event.GetEffectingButton() == EKeys::LeftMouseButton)
			return UWidgetBlueprintLibrary::DetectDragIfPressed(Event, this, EKeys::LeftMouseButton).NativeReply;
		if (Event.GetEffectingButton() == EKeys::RightMouseButton)
		{
			Item->OwningInventory->UseItem(Item);
			return FReply::Handled();
		}
	}
	return Super::NativeOnMouseButtonDown(Geometry, Event);
}

void URPG_ItemWidget::NativeOnDragDetected(const FGeometry& Geometry, const FPointerEvent& Event, UDragDropOperation*& Operation)
{
	if (bHandleInventoryInput) Operation = UDTInventoryDragDrop::BeginItemDrag(this, Item, false);
	else Super::NativeOnDragDetected(Geometry, Event, Operation);
}
