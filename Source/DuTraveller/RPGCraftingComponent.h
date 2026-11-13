#pragma once

#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "RPGCraftingComponent.generated.h"

class UItemBlueprint;

/** Recipe Blueprint defaults own costs, output, presentation and consumable classes. */
USTRUCT(BlueprintType)
struct FRPGRecipeCatalogueRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSubclassOf<UItemBlueprint> RecipeClass;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRPGRecipesChanged);

/** Legacy crafting component; the crafting menu pulls its recipe list through GetRecipes. */
UCLASS(ClassGroup = (Crafting), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class DUTRAVELLER_API URPGCraftingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URPGCraftingComponent();
	virtual void BeginPlay() override;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DT|Crafting Catalogue") TObjectPtr<UDataTable> RecipeCatalogue;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="DT|Crafting Catalogue") bool InitializeRecipeCatalogue();
	virtual void ReadyForReplication() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, ReplicatedUsing=OnRep_Recipes, Category = "Crafting")
	TArray<TObjectPtr<UItemBlueprint>> Recipes;

	UPROPERTY(BlueprintAssignable, Category = "Crafting")
	FRPGRecipesChanged OnRecipesChanged;

	UFUNCTION(BlueprintPure, Category = "Crafting")
	TArray<UItemBlueprint*> GetRecipes() const;

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	void Craft(UItemBlueprint* Recipe);
	bool TryCraftRecipe(int32 RecipeIndex);
private:
	UFUNCTION() void OnRep_Recipes();
	UFUNCTION(Server, Reliable) void ServerCraftRecipe(int32 RecipeIndex);
};
