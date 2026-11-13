#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ItemObject.generated.h"

class UInventoryComponent;
class URPG_ItemToolTip;
class UTexture2D;

UENUM(BlueprintType)
enum class EItemRarity : uint8
{
	IR_Common UMETA(DisplayName = "Common"),
	IR_Uncommon UMETA(DisplayName = "Uncommon"),
	IR_Rare UMETA(DisplayName = "Rare"),
	IR_Epic UMETA(DisplayName = "Epic"),
	IR_Legendary UMETA(DisplayName = "Legendary")
};

UENUM(BlueprintType)
enum class EEquipableSlot : uint8
{
	EIS_None UMETA(DisplayName = "None"),
	EIS_Helmet UMETA(DisplayName = "Helmet"),
	EIS_Head UMETA(DisplayName = "Head"),
	EIS_Vest UMETA(DisplayName = "Vest"),
	EIS_Hands UMETA(DisplayName = "Hands"),
	EIS_Legs UMETA(DisplayName = "Legs"),
	EIS_Feet UMETA(DisplayName = "Feet"),
	EIS_Backpack UMETA(DisplayName = "Backpack"),
	EIS_Grimoire UMETA(DisplayName = "Grimoire"),
	EIS_PrimaryWeapon UMETA(DisplayName = "Primary Weapon"),
	EIS_SecondaryWeapon UMETA(DisplayName = "Secondary Weapon"),
	EIS_Chest UMETA(DisplayName = "Chest"),
	EIS_Throwable UMETA(DisplayName = "Throwable")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnItemModified);

/** Runtime item stack used by the legacy inventory, vendor, crafting, and equipment UI. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class DUTRAVELLER_API UItemObject : public UObject
{
	GENERATED_BODY()

public:
	UItemObject();
	virtual bool IsSupportedForNetworking() const override { return true; }
	virtual UWorld* GetWorld() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Item")
	FGuid ItemId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText ItemDisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (MultiLine = true))
	FText ItemDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UTexture2D> Thumbnail;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TSubclassOf<URPG_ItemToolTip> ItemTooltip;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	EItemRarity Rarity = EItemRarity::IR_Common;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText UseActionText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	int32 Price = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (DisplayName = "Stackable"))
	bool bStackable = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1", EditCondition = "bStackable"))
	int32 MaxStackSize = 1;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_OwningInventory, Category = "Item")
	TObjectPtr<UInventoryComponent> OwningInventory;

	UPROPERTY(BlueprintAssignable, Category = "Item")
	FOnItemModified OnItemModified;

	UFUNCTION(BlueprintPure, Category = "Item")
	int32 GetQuantity() const { return Quantity; }

	UFUNCTION(BlueprintPure, Category = "Item|Presentation")
	FText GetResolvedDisplayName() const;

	UFUNCTION(BlueprintPure, Category = "Item|Presentation")
	FText GetItemTypeText() const;

	UFUNCTION(BlueprintPure, Category = "Item|Presentation")
	FText GetTooltipDetailsText() const;

	UFUNCTION(BlueprintPure, Category = "Item|Presentation")
	FText GetRarityText() const;

	UFUNCTION(BlueprintCallable, Category = "Item")
	void SetQuantity(int32 NewQuantity);

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Item")
	bool ShouldShowInInventory() const;
	virtual bool ShouldShowInInventory_Implementation() const;

	/** Return true only after a server-side consumable effect succeeds. Base items cannot be consumed. */
	UFUNCTION(BlueprintNativeEvent, Category = "Item")
	bool ApplyUseEffect(AActor* User);
	virtual bool ApplyUseEffect_Implementation(AActor* User);

	void Initialize(UInventoryComponent* NewOwningInventory, int32 NewQuantity);
	/** Local-only object used by retained UI slots, pickups and missing currency displays. */
	void InitializePresentationPlaceholder(int32 PresentationQuantity = 0);

private:
	UFUNCTION() void OnRep_Quantity();
	UFUNCTION() void OnRep_OwningInventory();
	UPROPERTY(VisibleAnywhere, ReplicatedUsing=OnRep_Quantity, BlueprintGetter = GetQuantity, Category = "Item")
	int32 Quantity = 1;
};
