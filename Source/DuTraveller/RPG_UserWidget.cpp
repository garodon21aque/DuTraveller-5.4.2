#include "RPG_UserWidget.h"
#include "RPGCharacter.h"
#include "InventoryComponent.h"
#include "RPGCraftingComponent.h"
#include "AttributeComponent.h"
#include "RPGPlayerController.h"
#include "UObject/UnrealType.h"
#include "RPG_AbilitySystemLibrary.h"
#include "RPG_SpellMenuWidgetController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ScrollBox.h"

void URPG_UserWidget::InitializePlayerContext()
{
	if (IsDesignTime()) return;
	APlayerController* OwnerController = GetOwningPlayer();
	ARPGCharacter* Avatar = OwnerController ? Cast<ARPGCharacter>(OwnerController->GetPawn()) : nullptr;
	const auto Assign = [this](FName Name, UObject* Value)
	{
		if (!Value) return;
		if (FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(GetClass(), Name))
			if (Value->IsA(Property->PropertyClass)) Property->SetObjectPropertyValue_InContainer(this, Value);
	};
	Assign(TEXT("PController"), OwnerController);
	Assign(TEXT("PlayerController"), OwnerController);
	if (Avatar)
	{
		Assign(TEXT("Inventory"), Avatar->InventoryComp);
		Assign(TEXT("InventoryComp"), Avatar->InventoryComp);
		Assign(TEXT("CraftingComp"), Avatar->CraftingComp);
		Assign(TEXT("AttributeComp"), Avatar->AttributeComp);
		Assign(TEXT("PC"), Avatar);
		Assign(TEXT("Player"), Avatar);
	}
}
void URPG_UserWidget::NativeOnInitialized() { InitializePlayerContext(); Super::NativeOnInitialized(); }
void URPG_UserWidget::NativePreConstruct() { InitializePlayerContext(); Super::NativePreConstruct(); }
void URPG_UserWidget::NativeConstruct()
{
	InitializePlayerContext();
	if (GetClass()->GetName().Contains(TEXT("SpellMenu"))) SetIsFocusable(true);
	Super::NativeConstruct();
}

FReply URPG_UserWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (ARPGPlayerController* PC = Cast<ARPGPlayerController>(GetOwningPlayer()))
		if (PC->ActivePanel == TEXT("Skills"))
			if (const int32* HotbarSlot = PC->HotbarKeys.Find(Event.GetKey()))
				if (URPG_SpellMenuWidgetController* Menu = URPG_AbilitySystemLibrary::GetSpellMenuWidgetController(this))
					if (Menu->IsWaitingForEquip()) { Menu->EquipSelectedToSlot(*HotbarSlot); return FReply::Handled(); }
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

FReply URPG_UserWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (ARPGPlayerController* PC = Cast<ARPGPlayerController>(GetOwningPlayer()))
		if (PC->ActivePanel == TEXT("Skills"))
			if (const int32* HotbarSlot = PC->HotbarKeys.Find(Event.GetEffectingButton()))
				if (URPG_SpellMenuWidgetController* Menu = URPG_AbilitySystemLibrary::GetSpellMenuWidgetController(this))
					if (Menu->IsWaitingForEquip()) { Menu->EquipSelectedToSlot(*HotbarSlot); return FReply::Handled(); }
	return Super::NativeOnMouseButtonDown(Geometry, Event);
}

FReply URPG_UserWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	ARPGPlayerController* PC = Cast<ARPGPlayerController>(GetOwningPlayer());
	if (PC && (PC->ActivePanel == TEXT("Skills") || PC->ActivePanel == TEXT("Talents")) && WidgetTree)
	{
		const FKey Key = Event.GetKey();
		if (Key == EKeys::Up || Key == EKeys::Down || Key == EKeys::PageUp || Key == EKeys::PageDown || Key == EKeys::Home || Key == EKeys::End)
		{
			bool Scrolled = false;
			WidgetTree->ForEachWidget([&](UWidget* Widget)
			{
				if (UScrollBox* Scroll = Cast<UScrollBox>(Widget))
				{
					if (Key == EKeys::Home) Scroll->ScrollToStart();
					else if (Key == EKeys::End) Scroll->ScrollToEnd();
					else Scroll->SetScrollOffset(FMath::Max(0.f, Scroll->GetScrollOffset() +
						((Key == EKeys::Up || Key == EKeys::PageUp) ? -1.f : 1.f) *
						((Key == EKeys::PageUp || Key == EKeys::PageDown) ? 400.f : 80.f)));
					Scrolled = true;
				}
			});
			if (Scrolled) return FReply::Handled();
		}
	}
	return Super::NativeOnKeyDown(Geometry, Event);
}

void URPG_UserWidget::SetWidgetController(UObject* InWidgetController)
{
	WidgetController = InWidgetController;
	WidgetControllerSet();
}
