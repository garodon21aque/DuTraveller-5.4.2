#include "DTDropQuantityWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "ItemObject.h"
#include "UObject/UnrealType.h"

void UDTDropQuantityWidget::NativeConstruct()
{
	Super::NativeConstruct();
	QuantityInput = Cast<UEditableTextBox>(GetWidgetFromName(TEXT("QuantityBox")));
	QuantityPanel = Cast<UVerticalBox>(GetWidgetFromName(TEXT("QuantitySelector")));
	if (!QuantityPanel || !bBuildDefaultQuantityControls || GetWidgetFromName(TEXT("DT_QuantityControls"))) return;
	QuantityPanel->SetVisibility(ESlateVisibility::Collapsed);
	UHorizontalBox* Controls = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DT_QuantityControls"));
	QuantityPanel->AddChildToVerticalBox(Controls);
	const auto Button = [&](const TCHAR* Label)
	{
		UButton* Control = WidgetTree->ConstructWidget<UButton>();
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
		Text->SetText(FText::FromString(Label));
		Control->SetContent(Text);
		Controls->AddChildToHorizontalBox(Control);
		return Control;
	};
	Button(TEXT("  −  "))->OnClicked.AddUniqueDynamic(this, &ThisClass::DecreaseQuantity);
	Button(TEXT("  +  "))->OnClicked.AddUniqueDynamic(this, &ThisClass::IncreaseQuantity);
	Button(TEXT("  Drop  "))->OnClicked.AddUniqueDynamic(this, &ThisClass::ConfirmQuantity);
	Button(TEXT("  Cancel  "))->OnClicked.AddUniqueDynamic(this, &ThisClass::CancelQuantity);
}
void UDTDropQuantityWidget::AdjustQuantity(int32 Delta)
{
	if (!QuantityInput) return;
	int32 Maximum = 1;
	if (FObjectPropertyBase* PayloadProperty = FindFProperty<FObjectPropertyBase>(GetClass(), TEXT("PendingPayload")))
		if (UObject* Payload = PayloadProperty->GetObjectPropertyValue_InContainer(this))
			if (FObjectPropertyBase* ItemProperty = FindFProperty<FObjectPropertyBase>(Payload->GetClass(), TEXT("Item")))
				if (UItemObject* Item = Cast<UItemObject>(ItemProperty->GetObjectPropertyValue_InContainer(Payload))) Maximum = FMath::Max(1, Item->GetQuantity());
	const int32 Current = FCString::Atoi(*QuantityInput->GetText().ToString());
	const int32 Next = static_cast<int32>(FMath::Clamp<int64>(static_cast<int64>(Current) + Delta, 1, Maximum));
	QuantityInput->SetText(FText::AsNumber(Next));
}
void UDTDropQuantityWidget::IncreaseQuantity() { AdjustQuantity(1); }
void UDTDropQuantityWidget::DecreaseQuantity() { AdjustQuantity(-1); }
void UDTDropQuantityWidget::ConfirmQuantity()
{
	if (!QuantityInput) return;
	AdjustQuantity(0);
	if (UFunction* Function = FindFunction(TEXT("FinishQuantityDrop")))
	{
		struct { int32 Quantity; } Parameters{FCString::Atoi(*QuantityInput->GetText().ToString())};
		ProcessEvent(Function, &Parameters);
	}
}
void UDTDropQuantityWidget::CancelQuantity()
{
	if (QuantityPanel) QuantityPanel->SetVisibility(ESlateVisibility::Collapsed);
	if (FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(GetClass(), TEXT("PendingPayload"))) Property->SetObjectPropertyValue_InContainer(this, nullptr);
}
