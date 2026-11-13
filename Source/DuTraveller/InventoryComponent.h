#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryComponent.generated.h"

class UItemObject;

USTRUCT(BlueprintType)
struct FRPGInventoryAmount
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TSubclassOf<UItemObject> ItemClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta=(ClampMin="1")) int32 Quantity = 1;
};

UENUM(BlueprintType, meta = (ScriptName = "EItemAddResultType"))
enum class EItemAddResult : uint8
{
	IAR_NoItemsAdded UMETA(DisplayName = "No Items Added"),
	IAR_SomeItemsAdded UMETA(DisplayName = "Some Items Added"),
	IAR_AllItemsAdded UMETA(DisplayName = "All Items Added")
};

USTRUCT(BlueprintType)
struct DUTRAVELLER_API FItemAddResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 AmountToGive = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 AmountGiven = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	EItemAddResult Result = EItemAddResult::IAR_NoItemsAdded;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FText ErrorText;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryUpdated);

/** Stack-based inventory contract retained by the original DuTraveller UI. */
UCLASS(ClassGroup = (Inventory), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class DUTRAVELLER_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();
	virtual void ReadyForReplication() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Inventory", meta = (ClampMin = "1"))
	int32 Capacity = 20;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, ReplicatedUsing=OnRep_Items, Category = "Inventory")
	TArray<TObjectPtr<UItemObject>> Items;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryUpdated OnInventoryUpdated;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetCapacity() const { return Capacity; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UItemObject* FindItemByClass(TSubclassOf<UItemObject> ItemClass);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<UItemObject*> FindItemsByClass(TSubclassOf<UItemObject> ItemClass) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UItemObject* FindItem(UItemObject* Item) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UItemObject* GetItem(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	FItemAddResult TryAddItemFromClass(TSubclassOf<UItemObject> ItemClass, int32 Quantity = 1);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool RemoveItem(UItemObject* Item);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool UseItem(UItemObject* Item);

	/** Authority-only exact consumption. Invalid quantities or foreign stacks are rejected. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool ConsumeItem(UItemObject* Item, int32 Quantity);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UItemObject* FindItemById(FGuid ItemId) const;

	/** Exact-class quantities used for currencies, recipes and server-validated purchases. */
	UFUNCTION(BlueprintPure, Category="Inventory")
	int64 CountItemsOfExactClass(TSubclassOf<UItemObject> ItemClass) const;
	/** Preflight all costs and outputs before committing. Failure leaves inventory unchanged. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Inventory")
	bool TryExchangeItems(const TArray<FRPGInventoryAmount>& Costs, const TArray<FRPGInventoryAmount>& Outputs);

private:
	UFUNCTION() void OnRep_Items();
	UFUNCTION(Server, Reliable) void ServerUseItem(UItemObject* Item);
	bool bMutationInProgress = false;
	bool RemoveItemInternal(UItemObject* Item);
	bool ConsumeItemInternal(UItemObject* Item, int32 Quantity);
	void NotifyInventoryUpdated();
	UPROPERTY(Transient)
	TMap<TSubclassOf<UItemObject>, TObjectPtr<UItemObject>> EmptyCurrencyPresentationItems;
	UPROPERTY(Transient)
	TObjectPtr<UItemObject> EmptySlotPresentationItem;
};
