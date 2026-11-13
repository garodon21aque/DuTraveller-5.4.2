#pragma once

#include "RPG_UserWidget.h"
#include "RPG_ItemWidget.generated.h"

class UInventoryComponent;
class UItemBlueprint;
class UItemObject;
class URPG_ItemToolTip;
class UTexture2D;

/** Legacy inventory/crafting item widget base. */
UCLASS(Blueprintable, BlueprintType)
class DUTRAVELLER_API URPG_ItemWidget : public URPG_UserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Inventory") bool bHandleInventoryInput = false;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void NativeOnDragDetected(const FGeometry& Geometry, const FPointerEvent& Event, UDragDropOperation*& Operation) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = "true"), Category = "Inventory")
	TObjectPtr<UItemObject> Item;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = "true"), Category = "Inventory")
	TObjectPtr<UInventoryComponent> Inventory;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = "true"), Category = "Inventory")
	TObjectPtr<UInventoryComponent> InventoryComp;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = "true"), Category = "Item")
	FText ItemDisplayName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = "true"), Category = "Item")
	TSubclassOf<URPG_ItemToolTip> ItemTooltip;

	/** Compatibility input used by the retained crafting and inventory Create Widget nodes. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = "true"), Category = "Recipe")
	TObjectPtr<UItemBlueprint> Recipe;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetItem(UItemObject* NewItem);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RefreshItemPresentation();

private:
	void EnsurePresentationItem();
	void UpdateItemVisuals();

	UFUNCTION()
	void HandleItemModified();

	UPROPERTY(Transient)
	TObjectPtr<URPG_ItemToolTip> RuntimeTooltip;

	UPROPERTY(Transient)
	TObjectPtr<UItemObject> PresentationPlaceholder;
};
