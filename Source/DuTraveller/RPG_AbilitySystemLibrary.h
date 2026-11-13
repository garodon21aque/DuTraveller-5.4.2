#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "RPG_AbilitySystemLibrary.generated.h"

class URPGAttributeMenuWidgetController;
class URPG_SpellMenuWidgetController;
class URPGOverlayWidgetController;
class URPGAttributeSet;
class URPGAttributeComponent;

/** Blueprint accessors retained for the legacy GAS UI graphs. */
UCLASS()
class DUTRAVELLER_API URPG_AbilitySystemLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="DT|UI", meta=(WorldContext="WorldContextObject", DefaultToSelf="WorldContextObject"))
	static void ToggleAttributesPanel(const UObject* WorldContextObject);
	UFUNCTION(BlueprintCallable, Category="DT|UI", meta=(WorldContext="WorldContextObject", DefaultToSelf="WorldContextObject"))
	static void ClosePlayerPanels(const UObject* WorldContextObject);
	UFUNCTION(BlueprintPure, Category = "RPG Ability System|Widget Controller", meta = (WorldContext = "WorldContextObject"))
	static URPGAttributeMenuWidgetController* GetAttributeWidgetController(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "RPG Ability System|Widget Controller", meta = (WorldContext = "WorldContextObject"))
	static URPG_SpellMenuWidgetController* GetSpellMenuWidgetController(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "RPG Ability System|Widget Controller", meta = (WorldContext = "WorldContextObject"))
	static URPGOverlayWidgetController* GetOverlayWidgetController(const UObject* WorldContextObject);

	/** Returns the owning local player's replicated GAS attributes for legacy HUD bindings. */
	UFUNCTION(BlueprintPure, Category = "RPG Ability System|Attributes", meta = (WorldContext = "WorldContextObject"))
	static URPGAttributeSet* GetPlayerAttributeSet(const UObject* WorldContextObject);

	/** Legacy float view mirrored from the replicated GAS AttributeSet on the owning pawn. */
	UFUNCTION(BlueprintPure, Category = "RPG Ability System|Attributes", meta = (WorldContext = "WorldContextObject"))
	static URPGAttributeComponent* GetPlayerAttributeView(const UObject* WorldContextObject);
};
