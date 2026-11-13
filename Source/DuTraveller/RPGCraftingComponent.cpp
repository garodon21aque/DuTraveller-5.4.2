#include "RPGCraftingComponent.h"

#include "InventoryComponent.h"
#include "ItemBlueprint.h"
#include "Net/UnrealNetwork.h"

URPGCraftingComponent::URPGCraftingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	bReplicateUsingRegisteredSubObjectList = true;
}

void URPGCraftingComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(URPGCraftingComponent, Recipes, COND_OwnerOnly);
}

void URPGCraftingComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner() && GetOwner()->HasAuthority()) InitializeRecipeCatalogue();
}

bool URPGCraftingComponent::InitializeRecipeCatalogue()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
	if (!RecipeCatalogue) return true;
	if (RecipeCatalogue->GetRowStruct() != FRPGRecipeCatalogueRow::StaticStruct() || RecipeCatalogue->GetRowMap().Num() > 256)
	{
		UE_LOG(LogTemp, Error, TEXT("Crafting catalogue has incompatible row type or excessive rows: %s"), *RecipeCatalogue->GetPathName());
		return false;
	}
	TSet<UClass*> Seen;
	for (UItemBlueprint* Recipe : Recipes) if (IsValid(Recipe)) Seen.Add(Recipe->GetClass());
	TArray<FName> Names = RecipeCatalogue->GetRowNames(); Names.Sort(FNameLexicalLess());
	for (FName Name : Names)
	{
		const FRPGRecipeCatalogueRow* Row = RecipeCatalogue->FindRow<FRPGRecipeCatalogueRow>(Name, TEXT("Crafting catalogue"));
		if (!Row || !Row->RecipeClass || Row->RecipeClass->HasAnyClassFlags(CLASS_Abstract) || Seen.Contains(Row->RecipeClass.Get())) continue;
		const UItemBlueprint* Defaults = Row->RecipeClass->GetDefaultObject<UItemBlueprint>();
		if (!Defaults->CraftedItem || Defaults->CraftedQuantity < 1) continue;
		UItemBlueprint* Recipe = NewObject<UItemBlueprint>(this, Row->RecipeClass);
		Recipes.Add(Recipe); Seen.Add(Row->RecipeClass.Get());
		if (IsReadyForReplication()) AddReplicatedSubObject(Recipe, COND_OwnerOnly);
	}
	OnRep_Recipes(); GetOwner()->ForceNetUpdate();
	return true;
}

void URPGCraftingComponent::ReadyForReplication()
{
	Super::ReadyForReplication();
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	for (UItemBlueprint* Recipe : Recipes)
		if (IsValid(Recipe)) AddReplicatedSubObject(Recipe, COND_OwnerOnly);
}

void URPGCraftingComponent::OnRep_Recipes()
{
	OnRecipesChanged.Broadcast();
}

TArray<UItemBlueprint*> URPGCraftingComponent::GetRecipes() const
{
	TArray<UItemBlueprint*> Result;
	Result.Reserve(Recipes.Num());
	for (UItemBlueprint* Recipe : Recipes)
	{
		Result.Add(Recipe);
	}
	return Result;
}

void URPGCraftingComponent::Craft(UItemBlueprint* Recipe)
{
	if (!GetOwner()) return;
	const int32 Index = Recipes.IndexOfByKey(Recipe);
	if (Index == INDEX_NONE) return;
	if (GetOwner()->HasAuthority()) TryCraftRecipe(Index);
	else ServerCraftRecipe(Index);
}

void URPGCraftingComponent::ServerCraftRecipe_Implementation(const int32 RecipeIndex)
{
	TryCraftRecipe(RecipeIndex);
}

bool URPGCraftingComponent::TryCraftRecipe(const int32 RecipeIndex)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Recipes.IsValidIndex(RecipeIndex)) return false;
	const UItemBlueprint* Recipe = Recipes[RecipeIndex];
	if (!IsValid(Recipe) || !Recipe->CraftedItem || Recipe->CraftedQuantity <= 0) return false;
	UInventoryComponent* Inventory = GetOwner()->FindComponentByClass<UInventoryComponent>();
	if (!Inventory) return false;
	TArray<FRPGInventoryAmount> Costs = Recipe->Ingredients;
	if (Costs.IsEmpty())
	{
		for (const TSubclassOf<UItemObject>& Required : Recipe->RequiredItems)
		{
			FRPGInventoryAmount Cost;
			Cost.ItemClass = Required; Cost.Quantity = 1;
			Costs.Add(Cost);
		}
	}
	// Empty recipes need an explicit designer decision rather than creating free items.
	if (Costs.IsEmpty()) return false;
	FRPGInventoryAmount Output;
	Output.ItemClass = Recipe->CraftedItem; Output.Quantity = Recipe->CraftedQuantity;
	return Inventory->TryExchangeItems(Costs, {Output});
}
