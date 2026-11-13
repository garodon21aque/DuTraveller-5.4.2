#include "DTPlayerInventoryWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Materials/MaterialInterface.h"
#include "Components/Button.h"
#include "Components/WrapBox.h"
#include "RPGCharacter.h"
#include "RPG_ItemWidget.h"
#include "InventoryComponent.h"
#include "Equipment_ItemObject.h"
#include "Currency_ItemObject.h"
#include "DTInventoryDragDrop.h"
#include "RPG_AbilitySystemLibrary.h"
#include "RPGAttributeMenuWidgetController.h"
#include "UObject/UnrealType.h"

void UDTPlayerInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshCharacterPreview();
	if (ARPGCharacter* Character = Cast<ARPGCharacter>(GetOwningPlayerPawn()))
	{
		BoundInventory = Character->InventoryComp;
		BoundInventory->OnInventoryUpdated.AddUniqueDynamic(this, &ThisClass::RefreshInventoryItems);
		Character->OnEquipedItemsChanged.AddUniqueDynamic(this, &ThisClass::RefreshInventoryItems);
	}
	if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT("AllBTN")))) Button->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowAllItems);
	if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT("WeaponsBTN")))) Button->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowWeapons);
	if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT("EquipmentBTN")))) Button->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowEquipment);
	if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT("MiscBTN")))) Button->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowMiscItems);
	RefreshInventoryItems();
	if (URPG_UserWidget* Attributes = Cast<URPG_UserWidget>(GetWidgetFromName(TEXT("WBP_AttributeMenu"))))
	{
		if (URPGAttributeMenuWidgetController* Model = URPG_AbilitySystemLibrary::GetAttributeWidgetController(this))
		{
			Attributes->SetWidgetController(Model);
			Model->BroadcastInitialValues();
		}
	}
}

void UDTPlayerInventoryWidget::NativeDestruct()
{
	if (BoundInventory)
	{
		BoundInventory->OnInventoryUpdated.RemoveAll(this);
		if (ARPGCharacter* Character = Cast<ARPGCharacter>(BoundInventory->GetOwner())) Character->OnEquipedItemsChanged.RemoveAll(this);
	}
	BoundInventory = nullptr;
	Super::NativeDestruct();
}

void UDTPlayerInventoryWidget::ShowAllItems() { ItemFilter = 0; RefreshInventoryItems(); }
void UDTPlayerInventoryWidget::ShowWeapons() { ItemFilter = 1; RefreshInventoryItems(); }
void UDTPlayerInventoryWidget::ShowEquipment() { ItemFilter = 2; RefreshInventoryItems(); }
void UDTPlayerInventoryWidget::ShowMiscItems() { ItemFilter = 3; RefreshInventoryItems(); }

void UDTPlayerInventoryWidget::RefreshInventoryItems()
{
	UWrapBox* Grid = Cast<UWrapBox>(GetWidgetFromName(TEXT("InventoryWB")));
	if (!Grid || !BoundInventory || !GetOwningPlayer()) return;
	UClass* ItemClass = LoadClass<URPG_ItemWidget>(nullptr, TEXT("/Game/UI/InventorySystem/WBP_ItemObject.WBP_ItemObject_C"));
	if (!ItemClass) return;
	Grid->ClearChildren();
	int32 Displayed = 0;
	for (UItemObject* Item : BoundInventory->Items)
	{
		if (!IsValid(Item) || !Item->ShouldShowInInventory() || Item->IsA<UCurrency_ItemObject>()) continue;
		const UEquipment_ItemObject* Gear = Cast<UEquipment_ItemObject>(Item);
		const bool bWeapon = Gear && (Gear->Slot == EEquipableSlot::EIS_PrimaryWeapon || Gear->Slot == EEquipableSlot::EIS_SecondaryWeapon || Gear->Slot == EEquipableSlot::EIS_Throwable);
		if ((ItemFilter == 1 && !bWeapon) || (ItemFilter == 2 && (!Gear || bWeapon)) || (ItemFilter == 3 && Gear)) continue;
		URPG_ItemWidget* Widget = CreateWidget<URPG_ItemWidget>(GetOwningPlayer(), ItemClass);
		Widget->bHandleInventoryInput = true;
		Widget->Inventory = BoundInventory;
		Widget->InventoryComp = BoundInventory;
		if (FObjectPropertyBase* Context = FindFProperty<FObjectPropertyBase>(Widget->GetClass(), TEXT("InventoryWidget")))
			if (IsA(Context->PropertyClass)) Context->SetObjectPropertyValue_InContainer(Widget, this);
		Widget->SetItem(Item);
		Grid->AddChildToWrapBox(Widget);
		++Displayed;
	}
	for (; Displayed < BoundInventory->Capacity; ++Displayed)
		Grid->AddChildToWrapBox(CreateWidget<URPG_ItemWidget>(GetOwningPlayer(), ItemClass));
}

bool UDTPlayerInventoryWidget::NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Operation)
{
	if (UItemObject* Item = UDTInventoryDragDrop::ResolveItem(Operation))
	{
		if (CharacterPreview && CharacterPreview->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition()))
		{
			UDTInventoryDragDrop::ApplyEquipmentDrop(this, Operation);
			return true;
		}
		if (UWidget* Grid = GetWidgetFromName(TEXT("InventoryWB")))
			if (Grid->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition()))
			{
				UDTInventoryDragDrop::ApplyEquipmentDrop(this, Operation, EEquipableSlot::EIS_None, true);
				return true;
			}
		return false; // Retain the existing world-drop confirmation outside these targets.
	}
	return Super::NativeOnDrop(Geometry, Event, Operation);
}

void UDTPlayerInventoryWidget::RefreshCharacterPreview()
{
	if (!WidgetTree) return;
	if (!CharacterPreview)
	{
		UCanvasPanel* Canvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
		if (!Canvas) return;
		CharacterPreview = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DT_CharacterPreview"));
		UCanvasPanelSlot* PreviewSlot = Canvas->AddChildToCanvas(CharacterPreview);
		PreviewSlot->SetZOrder(-1);
	}
	if (!PreviewMaterial) PreviewMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/UI/2D/Textures/MI_PlayerPreview.MI_PlayerPreview"));
	if (PreviewMaterial) CharacterPreview->SetBrushFromMaterial(PreviewMaterial);
	CharacterPreview->SetVisibility(bShowCharacterPreview ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* PreviewSlot = Cast<UCanvasPanelSlot>(CharacterPreview->Slot))
	{
		PreviewSlot->SetAnchors(PreviewAnchors);
		PreviewSlot->SetOffsets(PreviewOffsets);
	}
}
