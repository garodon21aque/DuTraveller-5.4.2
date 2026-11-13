#pragma once

#include "ItemObject.h"
#include "RPG_UserWidget.h"
#include "RPG_ItemToolTip.generated.h"

class UItemObject;
class URPG_ItemWidget;
class UTexture2D;

/** Legacy inventory tooltip widget base. */
UCLASS(Blueprintable, BlueprintType)
class DUTRAVELLER_API URPG_ItemToolTip : public URPG_UserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = "true"), Category = "Inventory")
	TObjectPtr<UItemObject> Item;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = "true"), Category = "Inventory")
	TObjectPtr<URPG_ItemWidget> InvItemWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = "true"), Category = "Item")
	FText ItemDisplayName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = "true", MultiLine = true), Category = "Item")
	FText ItemDescription;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	TObjectPtr<UTexture2D> ItemThumbnail;

	/** Exact compatibility name retained for older tooltip graphs. */
	UPROPERTY(BlueprintReadOnly, Category = "Item", meta=(ScriptName="LegacyStackable"))
	bool Stackable = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = "true"), Category = "Item")
	EItemRarity Rarity;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = "true", DisplayName = "Stackable"), Category = "Item")
	bool bStackable = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = "true"), Category = "Item")
	FText UseActionText;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = "true"), Category = "Item")
	int32 Price = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	int32 Quantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FText ItemTypeText;

	UPROPERTY(BlueprintReadOnly, Category = "Item", meta = (MultiLine = true))
	FText DetailsText;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FText RarityText;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetItem(UItemObject* NewItem);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RefreshFromItem();
};
