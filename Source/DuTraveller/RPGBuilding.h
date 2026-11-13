#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "InventoryComponent.h"
#include "RPGBuilding.generated.h"

UENUM(BlueprintType) enum class ERPGBuildingState : uint8 { Constructing, Active, Disabled };
UENUM(BlueprintType) enum class ERPGFarmState : uint8 { Empty, Growing, Ready };

UCLASS(BlueprintType)
class DUTRAVELLER_API URPGBuildingDefinition : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName DefinitionId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TSubclassOf<class ARPGBuilding> BuildingClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FRPGInventoryAmount> ConstructionCosts;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) int32 MaximumLevel = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float InteractionRange = 500.f;
	UFUNCTION(BlueprintPure) bool IsConfigured() const;
};

/** Replicated public settlement instance; private guild state remains on RPGGuildState. */
UCLASS(BlueprintType, Blueprintable)
class DUTRAVELLER_API ARPGBuilding : public AActor
{
	GENERATED_BODY()
public:
	ARPGBuilding();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	UPROPERTY(BlueprintReadOnly, Replicated) FGuid InstanceId;
	UPROPERTY(BlueprintReadOnly, Replicated) FGuid GuildId;
	UPROPERTY(BlueprintReadOnly, Replicated) FGuid SettlementId;
	UPROPERTY(BlueprintReadOnly, Replicated) TObjectPtr<URPGBuildingDefinition> Definition;
	UPROPERTY(BlueprintReadOnly, Replicated) int32 Level = 1;
	UPROPERTY(BlueprintReadOnly, Replicated) ERPGBuildingState State = ERPGBuildingState::Constructing;
	void InitializeBuilding(const FGuid& InGuildId, const FGuid& InSettlementId, URPGBuildingDefinition* InDefinition);
};

UCLASS(BlueprintType, Blueprintable)
class DUTRAVELLER_API ARPGFarmBuilding : public ARPGBuilding
{
	GENERATED_BODY()
public:
	ARPGFarmBuilding();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated) TSubclassOf<UItemObject> SeedItemClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated) TSubclassOf<UItemObject> HarvestItemClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, meta=(ClampMin="1")) int32 HarvestQuantity = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.1")) float GrowthDuration = 60.f;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Farm) ERPGFarmState FarmState = ERPGFarmState::Empty;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Farm) float ReadyServerTime = 0.f;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) bool Plant(class ARPGPlayerState* Requester);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) bool Harvest(class ARPGPlayerState* Requester);
	UFUNCTION(BlueprintImplementableEvent) void FarmStateChanged();
private:
	UFUNCTION() void OnRep_Farm();
};
