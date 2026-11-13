#pragma once

#include "ItemObject.h"
#include "Equipment_ItemObject.generated.h"

/** Equippable item specialization retained by inventory and drag/drop Blueprints. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class DUTRAVELLER_API UEquipment_ItemObject : public UItemObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Equipment") bool IsEquipped() const;
	virtual bool ShouldShowInInventory_Implementation() const override;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	EEquipableSlot Slot = EEquipableSlot::EIS_None;
	/** Rigid equipment attaches to a bone/socket without replacing the DT character mesh. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment|Appearance") TObjectPtr<class UStaticMesh> EquippedMesh;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment|Appearance") FName AttachSocket = TEXT("hand_r");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment|Appearance") FTransform AttachmentTransform;

	/** Additive GAS bonuses keyed by exact attribute name, e.g. Strength or MaxHealth.
	 * Values are configurable item data; no original balance values are assumed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Attributes")
	TMap<FName, float> AttributeBonuses;
};
