#pragma once

#include "RPG_UserWidget.h"
#include "Widgets/Layout/Anchors.h"
#include "DTPlayerInventoryWidget.generated.h"

class UImage;
class UMaterialInterface;
class UInventoryComponent;

/** Retains the authored inventory layout and adds the shared character capture. */
UCLASS(BlueprintType, Blueprintable)
class DUTRAVELLER_API UDTPlayerInventoryWidget : public URPG_UserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DT|Inventory Preview") bool bShowCharacterPreview = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DT|Inventory Preview") TObjectPtr<UMaterialInterface> PreviewMaterial;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DT|Inventory Preview") FAnchors PreviewAnchors = FAnchors(0.35f, 0.08f, 0.70f, 0.85f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DT|Inventory Preview") FMargin PreviewOffsets;
	UPROPERTY(Transient, BlueprintReadOnly, Category="DT|Inventory Preview") TObjectPtr<UImage> CharacterPreview;
	UFUNCTION(BlueprintCallable, Category="DT|Inventory Preview") void RefreshCharacterPreview();
	UFUNCTION(BlueprintCallable, Category="Inventory") void RefreshInventoryItems();
	UFUNCTION(BlueprintCallable, Category="Inventory") void ShowAllItems();
	UFUNCTION(BlueprintCallable, Category="Inventory") void ShowWeapons();
	UFUNCTION(BlueprintCallable, Category="Inventory") void ShowEquipment();
	UFUNCTION(BlueprintCallable, Category="Inventory") void ShowMiscItems();
public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual bool NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Operation) override;
private:
	UPROPERTY(Transient) TObjectPtr<UInventoryComponent> BoundInventory;
	uint8 ItemFilter = 0;
};
