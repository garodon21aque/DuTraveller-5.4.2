#include "DTUIRoot.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "RPGPlayerController.h"
#include "RPG_UserWidget.h"

FReply UDTUIRoot::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (ARPGPlayerController* PC = Cast<ARPGPlayerController>(GetOwningPlayer()))
		if (UUserWidget* Panel = PC->Panels.FindRef(PC->ActivePanel))
		{
			// Unhandled clicks outside the menu keep focus in the UI, including
			// mouse-button skill assignment, without reaching the game viewport.
			if (URPG_UserWidget* RPGPanel = Cast<URPG_UserWidget>(Panel)) RPGPanel->NativeOnMouseButtonDown(Geometry, Event);
			return FReply::Handled().SetUserFocus(Panel->TakeWidget(), EFocusCause::Mouse);
		}
	return Super::NativeOnMouseButtonDown(Geometry, Event);
}

FReply UDTUIRoot::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (!Event.IsRepeat() && !Event.IsControlDown() && !Event.IsAltDown() && !Event.IsShiftDown() && !Event.IsCommandDown())
		if (ARPGPlayerController* PC = Cast<ARPGPlayerController>(GetOwningPlayer()))
			if (PC->HandleUIPanelKey(Event.GetKey())) return FReply::Handled();
	return Super::NativeOnKeyDown(Geometry, Event);
}

void UDTUIRoot::AttachPlayerNotification(const UObject* WorldContextObject)
{
	UUserWidget* Widget = const_cast<UUserWidget*>(Cast<UUserWidget>(WorldContextObject));
	ARPGPlayerController* Controller = Widget ? Cast<ARPGPlayerController>(Widget->GetOwningPlayer()) : nullptr;
	if (Controller && Controller->UIRoot) Controller->UIRoot->AttachNotification(Widget);
}

void UDTUIRoot::EnsureLayers()
{
	if (HUDLayer || !WidgetTree) return;
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("DT_Root"));
	WidgetTree->RootWidget = Root;
	HUDLayer = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("DT_HUDLayer"));
	MenuLayer = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("DT_MenuLayer"));
	NotificationLayer = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("DT_NotificationLayer"));
	// Menus must remain above full-screen HUD and notification widgets.
	for (UOverlay* Layer : {HUDLayer.Get(), NotificationLayer.Get(), MenuLayer.Get()})
	{
		Layer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UOverlaySlot* LayerSlot = Root->AddChildToOverlay(Layer);
		LayerSlot->SetHorizontalAlignment(HAlign_Fill);
		LayerSlot->SetVerticalAlignment(VAlign_Fill);
	}
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	NotificationLayer->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UDTUIRoot::SetMenuInteractionActive(bool bActive)
{
	EnsureLayers();
	if (HUDLayer) HUDLayer->SetVisibility(bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::SelfHitTestInvisible);
	if (NotificationLayer) NotificationLayer->SetVisibility(ESlateVisibility::HitTestInvisible);
}

TSharedRef<SWidget> UDTUIRoot::RebuildWidget()
{
	EnsureLayers();
	return Super::RebuildWidget();
}

namespace
{
void AttachToLayer(UOverlay* Layer, UUserWidget* Widget)
{
	if (!Layer || !Widget || Widget->GetParent() == Layer) return;
	Widget->RemoveFromParent();
	UOverlaySlot* Slot = Layer->AddChildToOverlay(Widget);
	Slot->SetHorizontalAlignment(HAlign_Fill);
	Slot->SetVerticalAlignment(VAlign_Fill);
}
}
void UDTUIRoot::AttachHUD(UUserWidget* Widget) { EnsureLayers(); AttachToLayer(HUDLayer, Widget); }
void UDTUIRoot::AttachPanel(UUserWidget* Widget) { EnsureLayers(); AttachToLayer(MenuLayer, Widget); }
void UDTUIRoot::AttachNotification(UUserWidget* Widget) { EnsureLayers(); AttachToLayer(NotificationLayer, Widget); }
