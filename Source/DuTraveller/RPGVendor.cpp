#include "RPGVendor.h"

#include "ItemObject.h"
#include "Net/UnrealNetwork.h"
#include "RPGCharacter.h"

ARPGVendor::ARPGVendor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void ARPGVendor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARPGVendor, Offers);
}

int32 ARPGVendor::FindOfferIndex(const FName OfferId) const
{
	return Offers.IndexOfByPredicate([OfferId](const FRPGVendorOffer& Offer) { return Offer.OfferId == OfferId; });
}

bool ARPGVendor::TryPurchase(ARPGCharacter* Buyer, const FName OfferId, const int32 PurchaseCount)
{
	if (!HasAuthority() || !IsValid(Buyer) || !Buyer->InventoryComp || OfferId.IsNone() || PurchaseCount <= 0 ||
		FVector::DistSquared(Buyer->GetActorLocation(), GetActorLocation()) > FMath::Square(InteractionDistance)) return false;
	const int32 Index = FindOfferIndex(OfferId);
	if (!Offers.IsValidIndex(Index)) return false;
	FRPGVendorOffer& Offer = Offers[Index];
	if (!Offer.ItemClass || Offer.Quantity <= 0 || (Offer.RemainingStock >= 0 && Offer.RemainingStock < PurchaseCount)) return false;
	const int64 OutputQuantity = static_cast<int64>(Offer.Quantity) * PurchaseCount;
	if (OutputQuantity <= 0 || OutputQuantity > MAX_int32) return false;
	TArray<FRPGInventoryAmount> Costs;
	for (const FRPGInventoryAmount& Price : Offer.Price)
	{
		const int64 Total = static_cast<int64>(Price.Quantity) * PurchaseCount;
		if (!Price.ItemClass || Price.Quantity <= 0 || Total > MAX_int32) return false;
		FRPGInventoryAmount Cost = Price;
		Cost.Quantity = static_cast<int32>(Total);
		Costs.Add(Cost);
	}
	if (Costs.IsEmpty()) return false;
	FRPGInventoryAmount Output; Output.ItemClass = Offer.ItemClass; Output.Quantity = static_cast<int32>(OutputQuantity);
	if (!Buyer->InventoryComp->TryExchangeItems(Costs, {Output})) return false;
	if (Offer.RemainingStock >= 0) Offer.RemainingStock -= PurchaseCount;
	OnVendorChanged.Broadcast();
	ForceNetUpdate();
	return true;
}

void ARPGVendor::OnRep_Offers() { OnVendorChanged.Broadcast(); }
