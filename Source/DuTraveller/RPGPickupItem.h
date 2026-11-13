#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RPGPickupItem.generated.h"

class UItemObject;
class UWidgetComponent;
class URPGWorldUserWidget;
class ARPGCharacter;

/** Legacy world pickup actor; parent of the missing BP_MasterPickupItem/BP_Pickup Blueprint classes. */
UCLASS(BlueprintType, Blueprintable)
class DUTRAVELLER_API ARPGPickupItem : public AActor
{
	GENERATED_BODY()

public:
	ARPGPickupItem();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Local presentation object retained for the original interaction widgets. */
	UPROPERTY(Transient, Instanced, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UItemObject> Item;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_PickupData, Category="Item")
	TSubclassOf<UItemObject> ItemClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_PickupData, Category="Item", meta=(ClampMin="1"))
	int32 Quantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction", meta=(ClampMin="1.0"))
	float CollectionDistance = 300.f;

	/** World-space pickup prompt; WBP_ItemInteract is the intended widget class. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interaction")
	TObjectPtr<UWidgetComponent> PickupPrompt;

	/** Prompt shows while a local, living player pawn is inside this range. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction", meta=(ClampMin="1.0"))
	float PromptDistance = 320.f;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Item")
	bool InitializePickup(TSubclassOf<UItemObject> NewItemClass, int32 NewQuantity);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Item|Placement") bool bPlaceOnGround = true;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Item|Placement") bool PlaceOnGround();

	/** Authority-only atomic transfer. Any amount that does not fit remains in this pickup. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Item")
	bool TryCollect(ARPGCharacter* Collector);

private:
	UFUNCTION() void OnRep_PickupData();
	void RefreshPresentationItem();
	void PushItemToPrompt();
	float PromptCheckAccum = 0.f;
	bool bPromptInitialized = false;
};
