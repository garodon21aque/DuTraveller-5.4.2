#include "RPG_ItemToolTip.h"

#include "ItemObject.h"
#include "Equipment_ItemObject.h"
#include "RPG_ItemWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

void URPG_ItemToolTip::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshFromItem();
}

void URPG_ItemToolTip::SetItem(UItemObject* NewItem)
{
	Item = NewItem;
	// The retained Blueprint Construct graph reads InvItemWidget.Item. Equipment
	// slots have no inventory-entry widget, so provide that legacy data context.
	if (!InvItemWidget) InvItemWidget = NewObject<URPG_ItemWidget>(this);
	InvItemWidget->Item = NewItem;
	RefreshFromItem();
}

void URPG_ItemToolTip::RefreshFromItem()
{
	if (!Item)
	{
		ItemDisplayName = FText::GetEmpty();
		ItemDescription = FText::GetEmpty();
		ItemThumbnail = nullptr;
		Stackable = false;
		bStackable = false;
		UseActionText = FText::GetEmpty();
		Price = 0;
		Quantity = 0;
		ItemTypeText = FText::GetEmpty();
		DetailsText = FText::GetEmpty();
		RarityText = FText::GetEmpty();
		return;
	}
	ItemDisplayName = Item->GetResolvedDisplayName();
	ItemDescription = Item->ItemDescription;
	ItemThumbnail = Item->Thumbnail;
	Stackable = Item->bStackable;
	bStackable = Item->bStackable;
	UseActionText = Item->UseActionText;
	Price = Item->Price;
	Quantity = Item->GetQuantity();
	Rarity = Item->Rarity;
	ItemTypeText = Item->GetItemTypeText();
	DetailsText = Item->GetTooltipDetailsText();
	RarityText = Item->GetRarityText();
	const auto SetText = [this](FName Name, const FText& Text)
	{
		if (UTextBlock* Block = Cast<UTextBlock>(GetWidgetFromName(Name))) Block->SetText(Text);
	};
	TArray<FString> Description;
	if (!ItemDescription.IsEmpty()) Description.Add(ItemDescription.ToString());
	if (!DetailsText.IsEmpty()) Description.Add(DetailsText.ToString());
	if (const UEquipment_ItemObject* Gear = Cast<UEquipment_ItemObject>(Item))
		if (Gear->IsEquipped()) Description.Add(TEXT("Equipped"));
	if (Price > 0) Description.Add(FString::Printf(TEXT("Value: %d"), Price));
	SetText(TEXT("ItemName"), ItemDisplayName);
	SetText(TEXT("DescriptionTxT"), FText::FromString(FString::Join(Description, TEXT("\n"))));
	SetText(TEXT("RarityTxt"), RarityText);
	SetText(TEXT("ActionTxT"), UseActionText);
	SetText(TEXT("StackableText"), bStackable ? FText::Format(NSLOCTEXT("Inventory", "Quantity", "Quantity: {0}"), FText::AsNumber(Quantity)) : ItemTypeText);
	if (UImage* Image = Cast<UImage>(GetWidgetFromName(TEXT("Thumbnail")))) Image->SetBrushFromTexture(ItemThumbnail);
}
