#include "ItemBlueprint.h"

#include "Net/UnrealNetwork.h"

void UItemBlueprint::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UItemBlueprint, RequiredItems);
	DOREPLIFETIME(UItemBlueprint, Ingredients);
	DOREPLIFETIME(UItemBlueprint, CraftedItem);
	DOREPLIFETIME(UItemBlueprint, CraftedQuantity);
}
