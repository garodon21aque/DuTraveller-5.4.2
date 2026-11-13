#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InventoryComponent.h"
#include "RPGVendor.generated.h"

class ARPGCharacter;
class UItemObject;

USTRUCT(BlueprintType)
struct DUTRAVELLER_API FRPGVendorOffer
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName OfferId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TSubclassOf<UItemObject> ItemClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta=(ClampMin="1")) int32 Quantity = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FRPGInventoryAmount> Price;
	/** -1 means unlimited stock. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta=(ClampMin="-1")) int32 RemainingStock = -1;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRPGVendorChanged);

/** Server-owned pricing and stock parent for production vendor Blueprints. */
UCLASS(BlueprintType, Blueprintable)
class DUTRAVELLER_API ARPGVendor : public AActor
{
	GENERATED_BODY()
public:
	ARPGVendor();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Offers, SaveGame, Category="Vendor") TArray<FRPGVendorOffer> Offers;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vendor", meta=(ClampMin="1.0")) float InteractionDistance = 400.f;
	UPROPERTY(BlueprintAssignable, Category="Vendor") FRPGVendorChanged OnVendorChanged;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Vendor") bool TryPurchase(ARPGCharacter* Buyer, FName OfferId, int32 PurchaseCount = 1);
	UFUNCTION(BlueprintPure, Category="Vendor") int32 FindOfferIndex(FName OfferId) const;
private:
	UFUNCTION() void OnRep_Offers();
};
