#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RPGPlayerController.generated.h"

class URPG_UserWidget;
class URPGCraftingMenuWidget;
class URPGVendorMenuWidget;
class ARPGVendor;
class URPGPartyWidget;
class UUserWidget;
class UDTUIRoot;
class ARPGFarmBuilding;
class URPGBuildingDefinition;
class ARPGCharacter;

/** Legacy player-controller base; parent of the missing BP_RPGPlayerController Blueprint class. */
UCLASS(BlueprintType, Blueprintable)
class DUTRAVELLER_API ARPGPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARPGPlayerController();
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void AcknowledgePossession(APawn* P) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PlayerTick(float DeltaTime) override;
	UFUNCTION(Client, Reliable) void ClientShowDeath(float RespawnDelay);
	UFUNCTION(Client, Reliable) void ClientFinishRespawn();
	UPROPERTY(Transient, BlueprintReadOnly, Category="UI") TObjectPtr<URPG_UserWidget> DeathWidget;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="DT|Target Lock") FKey TargetLockKey;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Avoidance") FKey DodgeKey;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Avoidance") FKey FocusKey;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="DT|Target Lock", meta=(ClampMin="100")) float TargetLockRange = 2000.f;
	UPROPERTY(Transient, BlueprintReadOnly, Category="DT|Target Lock") TObjectPtr<ARPGCharacter> LockedCombatTarget;
	UPROPERTY(Transient, BlueprintReadOnly, Category="DT|Target Lock") TObjectPtr<class URPGWorldUserWidget> TargetStatusWidget;
	UFUNCTION(BlueprintCallable, Category="DT|Target Lock") void ToggleTargetLock();
	UFUNCTION(BlueprintCallable, Category="DT|Target Lock") void ClearTargetLock();
	UFUNCTION(BlueprintCallable, Category="Interaction") bool TryInteractWithPickup();
	UFUNCTION(BlueprintPure, Category="DT|Target Lock") bool IsTargetLockValid(ARPGCharacter* Target) const;
	UFUNCTION(BlueprintImplementableEvent, Category="DT|Target Lock") void TargetLockChanged(ARPGCharacter* Target);

	UPROPERTY(Transient, BlueprintReadOnly, Category="DT|UI") TObjectPtr<UDTUIRoot> UIRoot;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DT|UI") TMap<FName, TSoftClassPtr<UUserWidget>> PanelClasses;
	UPROPERTY(Transient, BlueprintReadOnly, Category="DT|UI") TMap<FName, TObjectPtr<UUserWidget>> Panels;
	UPROPERTY(BlueprintReadOnly, Category="DT|UI") FName ActivePanel;
	UFUNCTION(BlueprintCallable, Category="DT|UI") bool OpenUIPanel(FName PanelId);
	UFUNCTION(BlueprintCallable, Category="DT|UI") void ToggleUIPanel(FName PanelId);
	UFUNCTION(BlueprintCallable, Category="DT|UI") void CloseUIPanels();
	/** UI Only routes unhandled clicks to Slate rather than the game viewport. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="UI|Input") bool bUIOnlyInput = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="UI|Input") bool bBlockGameplayWhilePanelOpen = true;
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="UI|Input") void ApplyUIInputMode();
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category="UI|Input") bool ShouldBlockGameplayInput() const;
	UFUNCTION(BlueprintCallable, Category="UI|Input") bool HandleUIPanelKey(FKey Key);
	UFUNCTION(BlueprintCallable, Category="DT|UI") bool RegisterUIPanel(FName PanelId, UUserWidget* Widget);
	UFUNCTION(BlueprintCallable, Category="DT|UI") void ToggleSkills();
	UFUNCTION(BlueprintCallable, Category="DT|UI") void ToggleTalents();
	UFUNCTION(BlueprintCallable, Category="DT|UI") void ToggleQuests();
	UFUNCTION(BlueprintCallable, Category="DT|UI") void ToggleGuild();
	UFUNCTION(BlueprintCallable, Category="DT|UI") void ToggleAttributes();
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DT|Combat Input") bool bEnableCombatHotbarInput = true;
	/** Viewport key routing is independent of pawn mapping contexts that consume these keys. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="DT|Combat Input") TMap<FKey, int32> HotbarKeys;
	/** Shared by keyboard actions and Blueprint HUD buttons. The server validates the request. */
	UFUNCTION(BlueprintCallable, Category="DT|Combat Input") void ActivateHotbarSlot(int32 Slot);
	/** Override targeting in Blueprint without replacing skill ownership or execution. */
	UFUNCTION(BlueprintNativeEvent, Category="DT|Combat Input") ARPGCharacter* ResolveCombatTarget(int32 Slot) const;
	/** Designers select the buildable catalogue in the player-controller Blueprint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DT|Building") TArray<TObjectPtr<URPGBuildingDefinition>> BuildableDefinitions;
	UFUNCTION(BlueprintCallable, Category="DT|Farming") void RequestPlantFarm(ARPGFarmBuilding* Farm);
	UFUNCTION(BlueprintCallable, Category="DT|Farming") void RequestHarvestFarm(ARPGFarmBuilding* Farm);
	UFUNCTION(BlueprintCallable, Category="DT|Building") void RequestPlaceBuilding(URPGBuildingDefinition* Definition, FTransform Transform);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void TogglePauseMenu();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void InitializePlayerUI();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ToggleInventory();
	UFUNCTION(BlueprintCallable, Category="UI") void ToggleCrafting();
	UFUNCTION(BlueprintCallable, Category="UI") void OpenVendor(ARPGVendor* Vendor);
	UFUNCTION(BlueprintCallable, Category="UI") void CloseVendor();
	UFUNCTION(BlueprintCallable, Category="UI") void ToggleParty();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<URPG_UserWidget> HUDWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UUserWidget> InventoryWidgetClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI") TSubclassOf<URPGCraftingMenuWidget> CraftingWidgetClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI") TSubclassOf<URPGVendorMenuWidget> VendorWidgetClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI") TSubclassOf<URPGPartyWidget> PartyWidgetClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "UI")
	TObjectPtr<URPG_UserWidget> HUDWidget;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "UI")
	TObjectPtr<UUserWidget> PlayerInventoryWidget;
	UPROPERTY(Transient, BlueprintReadOnly, Category="UI") TObjectPtr<URPGCraftingMenuWidget> CraftingWidget;
	UPROPERTY(Transient, BlueprintReadOnly, Category="UI") TObjectPtr<URPGVendorMenuWidget> VendorWidget;
	UPROPERTY(Transient, BlueprintReadOnly, Category="UI") TObjectPtr<URPGPartyWidget> PartyWidget;

private:
	float LocalRespawnTime = 0.f;
	FTimerHandle UIInitializationTimer;
	TWeakObjectPtr<APawn> UIAvatar;
	bool bUIOwnsPause = false;
	float TargetOccludedSeconds = 0.f;
	bool bUIOwnsInputLock = false;
	void ConfigurePanel(FName PanelId, UUserWidget* Widget);
	UFUNCTION(Server, Reliable) void ServerFarmAction(ARPGFarmBuilding* Farm, bool bHarvest);
	UFUNCTION(Server, Reliable) void ServerPlaceBuilding(URPGBuildingDefinition* Definition, FTransform Transform);
};
