#include "RPGPlayerController.h"
#include "RPGPickupItem.h"
#include "DTUIRoot.h"
#include "DTGuildWidget.h"
#include "Blueprint/UserWidget.h"
#include "InventoryComponent.h"
#include "RPGCharacter.h"
#include "RPGPlayerCharacter.h"
#include "RPGCommerceWidgets.h"
#include "RPGPartyWidget.h"
#include "RPGPlayerState.h"
#include "RPGSkillComponent.h"
#include "RPGCombatTypes.h"
#include "RPGBuilding.h"
#include "RPGGuildState.h"
#include "RPGOverlayWidgetController.h"
#include "RPGAttributeMenuWidgetController.h"
#include "RPG_SpellMenuWidgetController.h"
#include "RPG_UserWidget.h"
#include "RPG_AbilitySystemLibrary.h"
#include "TimerManager.h"
#include "InputKeyEventArgs.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"
#include "Components/TextBlock.h"
#include "RPGWorldUserWidget.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
void AssignObject(UObject* Object, FName Name, UObject* Value)
{
	if (!Object || !Value) return;
	if (FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Object->GetClass(), Name))
		if (Value->IsA(Property->PropertyClass)) Property->SetObjectPropertyValue_InContainer(Object, Value);
}
}
void ARPGPlayerController::BeginPlay() { Super::BeginPlay(); InitializePlayerUI(); }
void ARPGPlayerController::AcknowledgePossession(APawn* NewPawn) { Super::AcknowledgePossession(NewPawn); InitializePlayerUI(); }

ARPGPlayerController::ARPGPlayerController()
{
	TargetLockKey = EKeys::Tab;
	DodgeKey=EKeys::LeftAlt;
	FocusKey=EKeys::Q;
	const FKey Keys[] = { EKeys::LeftMouseButton, EKeys::RightMouseButton, EKeys::One, EKeys::Two,
		EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine, EKeys::Zero };
	for (int32 Slot = 0; Slot < UE_ARRAY_COUNT(Keys); ++Slot) HotbarKeys.Add(Keys[Slot], Slot);
}

bool ARPGPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	if (const ARPGCharacter* Avatar = Cast<ARPGCharacter>(GetPawn())) if (Avatar->bIsDead) return true;
	if (ARPGPlayerCharacter* Hero=Cast<ARPGPlayerCharacter>(GetPawn()))
	{
		if (Params.Key==FocusKey)
		{
			Super::InputKey(Params); // Preserve pressed-key tracking, including focus-loss flushing.
			Hero->SetFocusHeld(Params.Event!=IE_Released && !ShouldBlockGameplayInput() && !IsPaused());
			return true;
		}
		if (Params.Key==DodgeKey && Params.Event==IE_Pressed)
		{
			if (!ShouldBlockGameplayInput() && !IsPaused()) Hero->RequestDodge(FVector::ZeroVector);
			return true;
		}
	}
	if (Params.Event == IE_Pressed && IsLocalController())
	{
		if (Params.Key == EKeys::E && ActivePanel.IsNone() && !IsPaused() && TryInteractWithPickup()) return true;
		if (ActivePanel == TEXT("Skills"))
		{
			if (const int32* Slot = HotbarKeys.Find(Params.Key))
				if (URPG_SpellMenuWidgetController* Menu = URPG_AbilitySystemLibrary::GetSpellMenuWidgetController(this))
					if (Menu->IsWaitingForEquip()) { Menu->EquipSelectedToSlot(*Slot); return true; }
		}
		if (Params.Key == TargetLockKey && ActivePanel.IsNone() && !IsPaused())
		{
			ToggleTargetLock();
			return true;
		}
	}
	// Pass the same event to DT so locomotion/traversal retains its normal input path.
	if (!ActivePanel.IsNone() && Params.Event == IE_Pressed && HandleUIPanelKey(Params.Key)) return true;
	if (ShouldBlockGameplayInput()) return true;
	const bool bHandled = Super::InputKey(Params);
	if (Params.Event == IE_Pressed && IsLocalController() && !IsMoveInputIgnored())
		if (const int32* Slot = HotbarKeys.Find(Params.Key)) ActivateHotbarSlot(*Slot);
	return bHandled;
}

bool ARPGPlayerController::TryInteractWithPickup()
{
	ARPGCharacter* Avatar = Cast<ARPGCharacter>(GetPawn());
	if (!Avatar || Avatar->bIsDead || !ActivePanel.IsNone()) return false;
	FVector Origin; FRotator Aim; GetPlayerViewPoint(Origin, Aim);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DTPickupInteraction), false, Avatar);
	// A precise ray can reach a thin grounded item without the floor blocking a wide sweep first.
	GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Aim.Vector() * 1000.f, ECC_Visibility, Params);
	if (!Cast<ARPGPickupItem>(Hit.GetActor()))
		GetWorld()->SweepSingleByChannel(Hit, Origin, Origin + Aim.Vector() * 1000.f,
			FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(25.f), Params);
	if (Hit.bBlockingHit)
		if (ARPGPickupItem* Pickup = Cast<ARPGPickupItem>(Hit.GetActor()))
		{
			Avatar->CollectPickup(Pickup);
			return true;
		}
	return false;
}

void ARPGPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (!InputComponent) return;
	InputComponent->BindAction(TEXT("OpenInventory"), IE_Pressed, this, &ThisClass::ToggleInventory);
	InputComponent->BindAction(TEXT("OpenCrafting"), IE_Pressed, this, &ThisClass::ToggleCrafting);
	InputComponent->BindAction(TEXT("OpenParty"), IE_Pressed, this, &ThisClass::ToggleParty);
	InputComponent->BindAction(TEXT("OpenSkills"), IE_Pressed, this, &ThisClass::ToggleSkills);
	InputComponent->BindAction(TEXT("OpenTalents"), IE_Pressed, this, &ThisClass::ToggleTalents);
	InputComponent->BindAction(TEXT("OpenQuests"), IE_Pressed, this, &ThisClass::ToggleQuests);
	InputComponent->BindAction(TEXT("OpenGuild"), IE_Pressed, this, &ThisClass::ToggleGuild);
	InputComponent->BindAction(TEXT("OpenAttributes"), IE_Pressed, this, &ThisClass::ToggleAttributes);
	InputComponent->BindAction(TEXT("PauseMenu"), IE_Pressed, this, &ThisClass::TogglePauseMenu).bExecuteWhenPaused = true;
}

void ARPGPlayerController::ActivateHotbarSlot(int32 Slot)
{
	if (!bEnableCombatHotbarInput || ShouldBlockGameplayInput() || IsPaused()) return;
	ARPGPlayerState* State = GetPlayerState<ARPGPlayerState>();
	UE_LOG(LogRPGCombat, Log, TEXT("[Input] Hotbar slot %d received"), Slot);
	if (State && State->Skills) State->Skills->RequestActivateHotbar(Slot, ResolveCombatTarget(Slot));
}

ARPGCharacter* ARPGPlayerController::ResolveCombatTarget_Implementation(int32 Slot) const
{
	const ARPGPlayerState* State = GetPlayerState<ARPGPlayerState>();
	if (!State || !State->Skills || !GetPawn()) return nullptr;
	const FRPGHotbarSlot* Entry = State->Skills->Hotbar.FindByPredicate([Slot](const FRPGHotbarSlot& Value) { return Value.Slot == Slot; });
	if (!Entry || !Entry->Definition) return nullptr;
	if (IsTargetLockValid(LockedCombatTarget)) return LockedCombatTarget;
	// Free attacks aim along control rotation and resolve hits at release/collision.
	if (!Entry->Definition->bRequiresTarget) return nullptr;
	FVector Origin; FRotator Rotation;
	GetPlayerViewPoint(Origin, Rotation);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DTCombatAim), false, GetPawn());
	// Include the camera's distance behind the avatar; action validation still uses avatar range.
	const float Distance = Entry->Definition->Range + FVector::Distance(Origin, GetPawn()->GetActorLocation());
	GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Rotation.Vector() * Distance, ECC_Visibility, Params);
	return Cast<ARPGCharacter>(Hit.GetActor());
}

bool ARPGPlayerController::IsTargetLockValid(ARPGCharacter* Target) const
{
	const ARPGCharacter* Self = Cast<ARPGCharacter>(GetPawn());
	return Self && !Self->bIsDead && IsValid(Target) && Target != Self && !Target->bIsDead &&
		Target->FactionId != FName("Neutral") &&
		(Self->FactionId.IsNone() || Self->FactionId != Target->FactionId) &&
		FVector::DistSquared(Self->GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(TargetLockRange);
}

void ARPGPlayerController::ClearTargetLock()
{
	if (TargetStatusWidget) TargetStatusWidget->RemoveFromParent();
	TargetStatusWidget = nullptr;
	if (!LockedCombatTarget) return;
	LockedCombatTarget = nullptr;
	TargetOccludedSeconds = 0.f;
	TargetLockChanged(nullptr);
}

void ARPGPlayerController::ToggleTargetLock()
{
	if (LockedCombatTarget) { ClearTargetLock(); return; }
	FVector Eye; FRotator View;
	GetPlayerViewPoint(Eye, View);
	ARPGCharacter* Best = nullptr;
	float BestScore = -1.f;
	for (TActorIterator<ARPGCharacter> It(GetWorld()); It; ++It)
	{
		if (!IsTargetLockValid(*It) || !LineOfSightTo(*It)) continue;
		const FVector Offset = It->GetActorLocation() + FVector(0,0,50) - Eye;
		const float Dot = FVector::DotProduct(View.Vector(), Offset.GetSafeNormal());
		if (Dot < 0.5f) continue;
		const float Score = Dot - 0.15f * Offset.Size() / FMath::Max(100.f, TargetLockRange);
		if (Score > BestScore) { BestScore = Score; Best = *It; }
	}
	LockedCombatTarget = Best;
	if (Best && IsLocalController())
	{
		UClass* Class = LoadClass<URPGWorldUserWidget>(nullptr, TEXT("/Game/UI/World/WPB_TargetEnemy.WPB_TargetEnemy_C"));
		TargetStatusWidget = Class ? CreateWidget<URPGWorldUserWidget>(this, Class) : nullptr;
		if (TargetStatusWidget) { TargetStatusWidget->InitializeCharacterOverlay(Best, false); TargetStatusWidget->AddToPlayerScreen(6); }
	}
	TargetOccludedSeconds = 0.f;
	TargetLockChanged(Best);
}

void ARPGPlayerController::PlayerTick(const float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (ARPGPlayerCharacter* Hero=Cast<ARPGPlayerCharacter>(GetPawn()))
		if (Hero->bFocusHeld && !IsInputKeyDown(FocusKey)) Hero->SetFocusHeld(false);
	// Retained Blueprint exit buttons may hide/remove their panel directly.
	if (UUserWidget* Panel = Panels.FindRef(ActivePanel))
		if (!Panel->IsVisible() || !Panel->GetParent()) CloseUIPanels();
	if (DeathWidget)
		if (UTextBlock* Countdown = Cast<UTextBlock>(DeathWidget->GetWidgetFromName(TEXT("Time_TXT"))))
			Countdown->SetText(FText::AsNumber(FMath::Max(0, FMath::CeilToInt(LocalRespawnTime - GetWorld()->GetTimeSeconds()))));
	if (!IsLocalController() || !LockedCombatTarget) return;
	if (!IsTargetLockValid(LockedCombatTarget)) { ClearTargetLock(); return; }
	TargetOccludedSeconds = LineOfSightTo(LockedCombatTarget) ? 0.f : TargetOccludedSeconds + DeltaTime;
	if (TargetOccludedSeconds > 0.75f) { ClearTargetLock(); return; }
	if (!ActivePanel.IsNone() || IsPaused()) return;
	FVector Eye; FRotator View;
	GetPlayerViewPoint(Eye, View);
	const FRotator Aim = (LockedCombatTarget->GetActorLocation() + FVector(0,0,50) - Eye).Rotation();
	SetControlRotation(FMath::RInterpTo(GetControlRotation(), Aim, DeltaTime, 12.f));
}

void ARPGPlayerController::ClientShowDeath_Implementation(float RespawnDelay)
{
	ClearTargetLock();
	CloseUIPanels();
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	LocalRespawnTime = GetWorld()->GetTimeSeconds() + RespawnDelay;
	if (DeathWidget) DeathWidget->RemoveFromParent();
	UClass* Class = LoadClass<URPG_UserWidget>(nullptr, TEXT("/Game/UI/World/WBP_DeathWidget.WBP_DeathWidget_C"));
	DeathWidget = Class ? CreateWidget<URPG_UserWidget>(this, Class) : nullptr;
	if (DeathWidget)
	{
		if (FIntProperty* Time = FindFProperty<FIntProperty>(DeathWidget->GetClass(), TEXT("TimeRemain")))
			Time->SetPropertyValue_InContainer(DeathWidget, FMath::CeilToInt(RespawnDelay));
		DeathWidget->AddToViewport(100);
	}
}

void ARPGPlayerController::ClientFinishRespawn_Implementation()
{
	if (DeathWidget) DeathWidget->RemoveFromParent();
	DeathWidget = nullptr;
	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();
	InitializePlayerUI();
	ApplyUIInputMode();
}
void ARPGPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	if (DeathWidget) DeathWidget->RemoveFromParent();
	DeathWidget = nullptr;
	ClearTargetLock();
	GetWorldTimerManager().ClearTimer(UIInitializationTimer);
	CloseUIPanels();
	if (UIRoot) UIRoot->RemoveFromParent();
	Panels.Reset();
	Super::EndPlay(Reason);
}
void ARPGPlayerController::InitializePlayerUI()
{
	if (!IsLocalController()) return;
	URPGOverlayWidgetController* Overlay = URPG_AbilitySystemLibrary::GetOverlayWidgetController(this);
	if (!GetPawn() || !PlayerState || !Overlay)
	{
		GetWorldTimerManager().SetTimer(UIInitializationTimer, this, &ThisClass::InitializePlayerUI, 0.2f, false);
		return;
	}
	GetWorldTimerManager().ClearTimer(UIInitializationTimer);
	if (!UIRoot)
	{
		UIRoot = CreateWidget<UDTUIRoot>(this, UDTUIRoot::StaticClass(), TEXT("DT_RootHUD"));
		if (UIRoot) UIRoot->AddToPlayerScreen(0);
	}
	if (!UIRoot) return;
	if (!HUDWidgetClass) HUDWidgetClass = LoadClass<URPG_UserWidget>(nullptr, TEXT("/Game/UI/HUD/WBP_HUD.WBP_HUD_C"));
	if (!HUDWidget && HUDWidgetClass)
	{
		HUDWidget = CreateWidget<URPG_UserWidget>(this, HUDWidgetClass);
		if (HUDWidget)
		{
			// Retained HUD exit-button delegates expect an attribute-menu instance
			// during Construct. Give that instance the same centralized ownership.
			const auto* AttributeOverride = PanelClasses.Find(TEXT("Attributes"));
			UClass* AttributeClass = AttributeOverride ? AttributeOverride->LoadSynchronous() : LoadClass<UUserWidget>(nullptr, TEXT("/Game/UI/Attribute/WBP_AttributeMenu.WBP_AttributeMenu_C"));
			if (AttributeClass)
			{
				UUserWidget* AttributePanel = CreateWidget<UUserWidget>(this, AttributeClass);
				if (RegisterUIPanel(TEXT("Attributes"), AttributePanel)) AssignObject(HUDWidget, TEXT("WBP_Attributes"), AttributePanel);
			}
			HUDWidget->SetWidgetController(Overlay);
			UIRoot->AttachHUD(HUDWidget);
			Overlay->BroadcastInitialValues();
		}
	}
	const TPair<FName, const TCHAR*> Defaults[] = {
		{TEXT("Inventory"), TEXT("/Game/UI/InventorySystem/WBP_PlayerInventory.WBP_PlayerInventory_C")},
		{TEXT("Crafting"), TEXT("/Game/UI/CraftingSystem/WBP_CraftingMenu.WBP_CraftingMenu_C")},
		{TEXT("Party"), TEXT("/Game/UI/Party/WBP_PartyMenu.WBP_PartyMenu_C")},
		{TEXT("Vendor"), TEXT("/Game/UI/InventorySystem/WBP_NPCVendorShop.WBP_NPCVendorShop_C")},
		{TEXT("Skills"), TEXT("/Game/UI/SpellMenu/WBP_SpellMenu.WBP_SpellMenu_C")},
		{TEXT("Talents"), TEXT("/Game/UI/HUD/WBP_Talents.WBP_Talents_C")},
		{TEXT("Attributes"), TEXT("/Game/UI/Attribute/WBP_AttributeMenu.WBP_AttributeMenu_C")},
		{TEXT("Quests"), TEXT("/Game/UI/QuestSystem/WBP_QuestLog.WBP_QuestLog_C")},
		{TEXT("Pause"), TEXT("/Game/UI/Menu/WBP_PauseMenu.WBP_PauseMenu_C")},
		{TEXT("Settings"), TEXT("/Game/UI/Menu/WBP_Settings.WBP_Settings_C")},
	};
	for (const auto& Entry : Defaults)
		if (!PanelClasses.Contains(Entry.Key)) PanelClasses.Add(Entry.Key, TSoftClassPtr<UUserWidget>(FSoftObjectPath(Entry.Value)));
	if (UIAvatar.Get() != GetPawn())
	{
		UIAvatar = GetPawn();
		if (ARPGCharacter* Avatar = Cast<ARPGCharacter>(GetPawn())) Avatar->InitializePlayerCaptures();
		for (const auto& Entry : Panels) ConfigurePanel(Entry.Key, Entry.Value);
		if (HUDWidget) HUDWidget->SetWidgetController(Overlay);
	}
}
bool ARPGPlayerController::RegisterUIPanel(FName PanelId, UUserWidget* Widget)
{
	if (!UIRoot || PanelId.IsNone() || !Widget || Widget->GetOwningPlayer() != this) return false;
	if (UUserWidget* Previous = Panels.FindRef(PanelId))
		if (Previous != Widget) Previous->RemoveFromParent();
	Panels.Add(PanelId, Widget);
	Widget->SetIsFocusable(true);
	UIRoot->AttachPanel(Widget);
	Widget->SetVisibility(ESlateVisibility::Collapsed);
	ConfigurePanel(PanelId, Widget);
	return true;
}
void ARPGPlayerController::ConfigurePanel(FName PanelId, UUserWidget* Widget)
{
	ARPGCharacter* Avatar = Cast<ARPGCharacter>(GetPawn());
	if (Avatar)
	{
		AssignObject(Widget, TEXT("Inventory"), Avatar->InventoryComp);
		AssignObject(Widget, TEXT("InventoryComp"), Avatar->InventoryComp);
		AssignObject(Widget, TEXT("PC"), Avatar);
	}
	if (URPGCraftingMenuWidget* Craft = Cast<URPGCraftingMenuWidget>(Widget)) Craft->InitializeCrafting(Avatar);
	if (URPGPartyWidget* Party = Cast<URPGPartyWidget>(Widget)) Party->InitializeParty(GetPlayerState<ARPGPlayerState>());
	if (URPG_UserWidget* RPGWidget = Cast<URPG_UserWidget>(Widget))
	{
		if (PanelId == TEXT("Attributes"))
		{
			URPGAttributeMenuWidgetController* Model = URPG_AbilitySystemLibrary::GetAttributeWidgetController(this);
			RPGWidget->SetWidgetController(Model);
			if (Model) Model->BroadcastInitialValues();
		}
		if (PanelId == TEXT("Skills") || PanelId == TEXT("Talents"))
		{
			URPG_SpellMenuWidgetController* Model = URPG_AbilitySystemLibrary::GetSpellMenuWidgetController(this);
			RPGWidget->SetWidgetController(Model);
			if (Model) Model->BroadcastInitialValues();
		}
	}
	if (PanelId == TEXT("Inventory")) { PlayerInventoryWidget = Widget; AssignObject(this, TEXT("InventoryWidget"), Widget); }
	if (PanelId == TEXT("Crafting")) CraftingWidget = Cast<URPGCraftingMenuWidget>(Widget);
	if (PanelId == TEXT("Party")) PartyWidget = Cast<URPGPartyWidget>(Widget);
	if (PanelId == TEXT("Vendor")) VendorWidget = Cast<URPGVendorMenuWidget>(Widget);
}
bool ARPGPlayerController::OpenUIPanel(FName PanelId)
{
	if (!IsLocalController() || PanelId.IsNone()) return false;
	InitializePlayerUI();
	if (!UIRoot) return false;
	UUserWidget* Widget = Panels.FindRef(PanelId);
	if (!Widget)
	{
		UClass* Class = nullptr;
		if (PanelId == TEXT("Inventory") && InventoryWidgetClass) Class = InventoryWidgetClass;
		else if (PanelId == TEXT("Crafting") && CraftingWidgetClass) Class = CraftingWidgetClass;
		else if (PanelId == TEXT("Party") && PartyWidgetClass) Class = PartyWidgetClass;
		else if (PanelId == TEXT("Vendor") && VendorWidgetClass) Class = VendorWidgetClass;
		else if (const auto* SoftClass = PanelClasses.Find(PanelId)) Class = SoftClass->LoadSynchronous();
		if (!Class && PanelId == TEXT("Guild")) Class = UDTGuildWidget::StaticClass();
		if (!Class) return false;
		Widget = CreateWidget<UUserWidget>(this, Class);
		if (!RegisterUIPanel(PanelId, Widget)) return false;
	}
	CloseUIPanels();
	UIRoot->AttachPanel(Widget);
	ConfigurePanel(PanelId, Widget);
	ActivePanel = PanelId;
	Widget->SetVisibility(ESlateVisibility::Visible);
	if (PanelId == TEXT("Inventory")) if (ARPGCharacter* Avatar = Cast<ARPGCharacter>(GetPawn())) Avatar->FrontCaptureOn();
	if (PanelId == TEXT("Pause") && !IsPaused()) bUIOwnsPause = SetPause(true);
	ApplyUIInputMode();
	return true;
}
void ARPGPlayerController::CloseUIPanels()
{
	if (ARPGCharacter* Avatar = Cast<ARPGCharacter>(GetPawn())) { Avatar->FrontCaptureOff(); Avatar->FaceCaptureOff(); }
	for (const auto& Entry : Panels) if (Entry.Value) Entry.Value->SetVisibility(ESlateVisibility::Collapsed);
	ActivePanel = NAME_None;
	if (bUIOwnsPause) { SetPause(false); bUIOwnsPause = false; }
	ApplyUIInputMode();
}
bool ARPGPlayerController::ShouldBlockGameplayInput_Implementation() const
{
	return bBlockGameplayWhilePanelOpen && !ActivePanel.IsNone();
}

bool ARPGPlayerController::HandleUIPanelKey(FKey Key)
{
	if (ActivePanel.IsNone()) return false;
	if (Key == EKeys::Escape) { CloseUIPanels(); return true; }
	const TMap<FName, FName> ActionPanels = {
		{TEXT("OpenInventory"), TEXT("Inventory")}, {TEXT("OpenCrafting"), TEXT("Crafting")},
		{TEXT("OpenParty"), TEXT("Party")}, {TEXT("OpenSkills"), TEXT("Skills")},
		{TEXT("OpenTalents"), TEXT("Talents")}, {TEXT("OpenQuests"), TEXT("Quests")},
		{TEXT("OpenGuild"), TEXT("Guild")}, {TEXT("OpenAttributes"), TEXT("Attributes")}};
	for (const FInputActionKeyMapping& Mapping : GetDefault<UInputSettings>()->GetActionMappings())
		if (Mapping.Key == Key && !Mapping.bShift && !Mapping.bCtrl && !Mapping.bAlt && !Mapping.bCmd)
			if (const FName* Panel = ActionPanels.Find(Mapping.ActionName)) { ToggleUIPanel(*Panel); return true; }
	return false;
}

void ARPGPlayerController::ApplyUIInputMode_Implementation()
{
	const bool bLock = ShouldBlockGameplayInput();
	if (bLock != bUIOwnsInputLock)
	{
		SetIgnoreMoveInput(bLock);
		SetIgnoreLookInput(bLock);
		bUIOwnsInputLock = bLock;
		if (bLock)
		{
			if (ARPGPlayerCharacter* Hero=Cast<ARPGPlayerCharacter>(GetPawn())) Hero->CancelAvoidance();
			if (PlayerInput) PlayerInput->FlushPressedKeys();
			if (ACharacter* Avatar = Cast<ACharacter>(GetPawn())) Avatar->GetCharacterMovement()->StopMovementImmediately();
		}
	}
	bShowMouseCursor = !ActivePanel.IsNone();
	if (UIRoot) UIRoot->SetMenuInteractionActive(!ActivePanel.IsNone());
	if (UIRoot) UIRoot->SetVisibility(ActivePanel.IsNone() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Visible);
	if (UUserWidget* Widget = Panels.FindRef(ActivePanel))
	{
		if (bUIOnlyInput)
		{
			FInputModeUIOnly Mode;
			Mode.SetWidgetToFocus(Widget->TakeWidget());
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			SetInputMode(Mode);
		}
		else
		{
			FInputModeGameAndUI Mode;
			Mode.SetWidgetToFocus(Widget->TakeWidget());
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			Mode.SetHideCursorDuringCapture(false);
			SetInputMode(Mode);
		}
	}
	else SetInputMode(FInputModeGameOnly());
}
void ARPGPlayerController::ToggleUIPanel(FName PanelId) { if (ActivePanel == PanelId) CloseUIPanels(); else OpenUIPanel(PanelId); }
void ARPGPlayerController::ToggleInventory() { ToggleUIPanel(TEXT("Inventory")); }
void ARPGPlayerController::ToggleCrafting() { ToggleUIPanel(TEXT("Crafting")); }
void ARPGPlayerController::ToggleParty() { ToggleUIPanel(TEXT("Party")); }
void ARPGPlayerController::ToggleSkills() { ToggleUIPanel(TEXT("Skills")); }
void ARPGPlayerController::ToggleTalents() { ToggleUIPanel(TEXT("Talents")); }
void ARPGPlayerController::ToggleQuests() { ToggleUIPanel(TEXT("Quests")); }
void ARPGPlayerController::ToggleGuild() { ToggleUIPanel(TEXT("Guild")); }
void ARPGPlayerController::ToggleAttributes() { ToggleUIPanel(TEXT("Attributes")); }
void ARPGPlayerController::TogglePauseMenu() { if (!ActivePanel.IsNone()) CloseUIPanels(); else OpenUIPanel(TEXT("Pause")); }
void ARPGPlayerController::OpenVendor(ARPGVendor* Vendor)
{
	if (IsValid(Vendor) && OpenUIPanel(TEXT("Vendor")) && VendorWidget)
		VendorWidget->InitializeVendor(Vendor, Cast<ARPGCharacter>(GetPawn()));
}
void ARPGPlayerController::CloseVendor() { if (ActivePanel == TEXT("Vendor")) CloseUIPanels(); }

void ARPGPlayerController::RequestPlantFarm(ARPGFarmBuilding* Farm) { ServerFarmAction(Farm, false); }
void ARPGPlayerController::RequestHarvestFarm(ARPGFarmBuilding* Farm) { ServerFarmAction(Farm, true); }
void ARPGPlayerController::ServerFarmAction_Implementation(ARPGFarmBuilding* Farm, bool bHarvest)
{
	ARPGPlayerState* State = GetPlayerState<ARPGPlayerState>();
	if (!IsValid(Farm) || !State) return;
	// Existing farm operations verify guild access, range, seed cost and crop state.
	if (bHarvest) Farm->Harvest(State); else Farm->Plant(State);
}
void ARPGPlayerController::RequestPlaceBuilding(URPGBuildingDefinition* Definition, FTransform Transform)
{
	ServerPlaceBuilding(Definition, Transform);
}
void ARPGPlayerController::ServerPlaceBuilding_Implementation(URPGBuildingDefinition* Definition, FTransform Transform)
{
	ARPGPlayerState* State = GetPlayerState<ARPGPlayerState>();
	if (!State || !State->GuildState || !IsValid(Definition) || !BuildableDefinitions.Contains(Definition)) return;
	State->GuildState->PlaceBuilding(State, Definition, Transform);
}
