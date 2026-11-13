#include "RPG_AbilitySystemLibrary.h"
#include "RPGPlayerController.h"

#include "RPGAttributeMenuWidgetController.h"
#include "RPG_SpellMenuWidgetController.h"
#include "RPGOverlayWidgetController.h"
#include "RPGPlayerState.h"
#include "RPGAttributeSet.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "AttributeComponent.h"
#include "GameFramework/Pawn.h"

namespace
{
ARPGPlayerController* ResolveUIController(const UObject* Context)
{
	if (const UUserWidget* Widget = Cast<UUserWidget>(Context)) return Cast<ARPGPlayerController>(Widget->GetOwningPlayer());
	if (const APlayerController* Controller = Cast<APlayerController>(Context)) return const_cast<ARPGPlayerController*>(Cast<ARPGPlayerController>(Controller));
	return Cast<ARPGPlayerController>(UGameplayStatics::GetPlayerController(Context, 0));
}
ARPGPlayerState* ResolveRPGPlayerState(const UObject* WorldContextObject)
{
	APlayerController* PC = nullptr;
	if (const UUserWidget* Widget = Cast<UUserWidget>(WorldContextObject))
	{
		PC = Widget->GetOwningPlayer();
	}
	if (!PC)
	{
		PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
	}
	return PC ? PC->GetPlayerState<ARPGPlayerState>() : nullptr;
}
}

void URPG_AbilitySystemLibrary::ToggleAttributesPanel(const UObject* WorldContextObject)
{
	if (ARPGPlayerController* Controller = ResolveUIController(WorldContextObject)) Controller->ToggleAttributes();
}
void URPG_AbilitySystemLibrary::ClosePlayerPanels(const UObject* WorldContextObject)
{
	if (ARPGPlayerController* Controller = ResolveUIController(WorldContextObject)) Controller->CloseUIPanels();
}

URPGAttributeMenuWidgetController* URPG_AbilitySystemLibrary::GetAttributeWidgetController(const UObject* WorldContextObject)
{
	APlayerController* PC = nullptr;
	if (const UUserWidget* Widget = Cast<UUserWidget>(WorldContextObject)) PC = Widget->GetOwningPlayer();
	if (!PC) PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
	ARPGPlayerState* PS = PC ? PC->GetPlayerState<ARPGPlayerState>() : nullptr;
	if (!PS) return nullptr;
	if (!PS->AttributeWidgetController)
	{
		// Preserve the exact Blueprint identity expected by retained cast nodes.
		UClass* ControllerClass = LoadClass<URPGAttributeMenuWidgetController>(nullptr,
			TEXT("/Game/Blueprints/Controllers/BP_RPGAttributeWidgetController.BP_RPGAttributeWidgetController_C"));
		if (!ControllerClass) ControllerClass = URPGAttributeMenuWidgetController::StaticClass();
		PS->AttributeWidgetController = NewObject<URPGAttributeMenuWidgetController>(PS, ControllerClass);
	}
	FWidgetControllerParams Params;
	Params.PlayerController = PC; Params.PlayerState = PS;
	Params.AbilitySystemComponent = PS->AbilitySystem; Params.AttributeSet = PS->Attributes;
	PS->AttributeWidgetController->SetWidgetControllerParams(Params);
	PS->AttributeWidgetController->BindCallbacks();
	return PS->AttributeWidgetController;
}

URPG_SpellMenuWidgetController* URPG_AbilitySystemLibrary::GetSpellMenuWidgetController(const UObject* WorldContextObject)
{
	APlayerController* PC = Cast<UUserWidget>(WorldContextObject) ? Cast<UUserWidget>(WorldContextObject)->GetOwningPlayer() : nullptr;
	if (!PC) PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
	ARPGPlayerState* PS = PC ? PC->GetPlayerState<ARPGPlayerState>() : nullptr; if (!PS) return nullptr;
	if (!PS->SpellWidgetController)
	{
		UClass* Class = LoadClass<URPG_SpellMenuWidgetController>(nullptr, TEXT("/Game/Blueprints/Controllers/BP_RPGSpellWidgetController.BP_RPGSpellWidgetController_C"));
		PS->SpellWidgetController = NewObject<URPG_SpellMenuWidgetController>(PS, Class ? Class : URPG_SpellMenuWidgetController::StaticClass());
	}
	FWidgetControllerParams Params; Params.PlayerController = PC; Params.PlayerState = PS; Params.AbilitySystemComponent = PS->AbilitySystem; Params.AttributeSet = PS->Attributes;
	PS->SpellWidgetController->SetWidgetControllerParams(Params); PS->SpellWidgetController->BindCallbacks();
	return PS->SpellWidgetController;
}

URPGOverlayWidgetController* URPG_AbilitySystemLibrary::GetOverlayWidgetController(const UObject* WorldContextObject)
{
	APlayerController* PC = Cast<UUserWidget>(WorldContextObject) ? Cast<UUserWidget>(WorldContextObject)->GetOwningPlayer() : nullptr;
	if (!PC) PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
	ARPGPlayerState* PS = PC ? PC->GetPlayerState<ARPGPlayerState>() : nullptr; if (!PS) return nullptr;
	if (!PS->OverlayWidgetController)
	{
		UClass* Class = LoadClass<URPGOverlayWidgetController>(nullptr, TEXT("/Game/Blueprints/Controllers/BP_RPGOverlayWidgetController.BP_RPGOverlayWidgetController_C"));
		PS->OverlayWidgetController = NewObject<URPGOverlayWidgetController>(PS, Class ? Class : URPGOverlayWidgetController::StaticClass());
	}
	FWidgetControllerParams Params; Params.PlayerController = PC; Params.PlayerState = PS; Params.AbilitySystemComponent = PS->AbilitySystem; Params.AttributeSet = PS->Attributes;
	PS->OverlayWidgetController->SetWidgetControllerParams(Params); PS->OverlayWidgetController->BindCallbacks();
	return PS->OverlayWidgetController;
}

URPGAttributeSet* URPG_AbilitySystemLibrary::GetPlayerAttributeSet(const UObject* WorldContextObject)
{
	if (ARPGPlayerState* PS = ResolveRPGPlayerState(WorldContextObject))
	{
		return PS->Attributes;
	}
	return nullptr;
}

URPGAttributeComponent* URPG_AbilitySystemLibrary::GetPlayerAttributeView(const UObject* WorldContextObject)
{
	APawn* Pawn = nullptr;
	if (const UUserWidget* Widget = Cast<UUserWidget>(WorldContextObject))
	{
		Pawn = Widget->GetOwningPlayerPawn();
	}
	if (!Pawn)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0))
		{
			Pawn = PC->GetPawn();
		}
	}
	return Pawn ? Pawn->FindComponentByClass<URPGAttributeComponent>() : nullptr;
}
