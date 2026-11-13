#pragma once

#include "Blueprint/DragDropOperation.h"
#include "ItemObject.h"
#include "DTInventoryDragDrop.generated.h"

class UUserWidget;

/** Shared payload for retained inventory and equipment widgets. */
UCLASS()
class DUTRAVELLER_API UDTInventoryDragDrop : public UDragDropOperation
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly) TObjectPtr<UItemObject> DraggedItem;
	UPROPERTY(BlueprintReadOnly) bool bFromEquipment = false;
	UFUNCTION(BlueprintCallable, Category="Inventory")
	static UDTInventoryDragDrop* BeginItemDrag(UUserWidget* Source, UItemObject* Item, bool bEquipped);
	UFUNCTION(BlueprintPure, Category="Inventory")
	static UItemObject* ResolveItem(UDragDropOperation* Operation);
	UFUNCTION(BlueprintCallable, Category="Inventory")
	static bool ApplyEquipmentDrop(UUserWidget* Target, UDragDropOperation* Operation,
		EEquipableSlot RequiredSlot = EEquipableSlot::EIS_None, bool bUnequip = false);
	/** Runs the actual widget drag/drop handlers against laid-out PIE widgets. */
	UFUNCTION(BlueprintCallable, Category="Runtime Diagnostics")
	static bool DriveInventoryDrop(class APlayerController* Controller, UItemObject* Item, FName TargetName);
	UFUNCTION(BlueprintCallable, Category="Runtime Diagnostics")
	static UItemObject* CreateVerificationGear(class APlayerController* Controller, FText DisplayName,
		EEquipableSlot ItemSlot, const TMap<FName, float>& Bonuses);
};
