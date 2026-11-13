#pragma once

#include "RPG_UserWidget.h"
#include "ItemObject.h"
#include "DTEquipmentWidget.generated.h"

class ARPGCharacter;
class UEquipment_ItemObject;
class URPG_ItemToolTip;

/** Native interaction behind the existing WBP_EquipmentSlot layout. */
UCLASS()
class DUTRAVELLER_API UDTEquipmentSlotWidget : public URPG_UserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Equipment") EEquipableSlot EquipmentSlot = EEquipableSlot::EIS_None;
	UPROPERTY(BlueprintReadOnly, Transient) TObjectPtr<UEquipment_ItemObject> EquippedItem;
	UFUNCTION(BlueprintCallable, Category="Equipment") void RefreshEquipmentSlot();
public:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void NativeOnDragDetected(const FGeometry& Geometry, const FPointerEvent& Event, UDragDropOperation*& Operation) override;
	virtual bool NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Operation) override;
private:
	UPROPERTY(Transient) TObjectPtr<URPG_ItemToolTip> EquipmentTooltip;
};

/** Keeps the authored equipment panel synchronized with inventory replication. */
UCLASS()
class DUTRAVELLER_API UDTEquipmentWidget : public URPG_UserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Equipment") void RefreshEquipmentPanel();
public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual bool NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Operation) override;
private:
	UPROPERTY(Transient) TObjectPtr<ARPGCharacter> BoundCharacter;
};
