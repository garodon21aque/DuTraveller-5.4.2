#pragma once

#include "RPGSpawnArea.h"
#include "RPGDynamicQuestTypes.h"
#include "RPGRandomQuestSpawner.generated.h"

USTRUCT(BlueprintType)
struct FRPGQuestOffer
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FGuid OfferId;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<URPGRandomQuestDefinition> Definition;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<URPGQuestGroupDefinition> Group;
	UPROPERTY(BlueprintReadOnly) double ExpiresAt = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRPGQuestOffersChanged);

/** Place this volume within a location; it offers quests and can spawn linked actors. */
UCLASS(BlueprintType, Blueprintable)
class DUTRAVELLER_API ARPGRandomQuestSpawner : public ARPGSpawnArea
{
	GENERATED_BODY()
public:
	ARPGRandomQuestSpawner();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<TObjectPtr<URPGQuestGroupDefinition>> QuestGroups;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1", ClampMax="32")) int32 MaximumOffers = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) float OfferLifetime = 300.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.5")) float RefreshInterval = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bGenerateAutomatically = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bSingleConsumerOffers = false;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Offers) TArray<FRPGQuestOffer> Offers;
	UPROPERTY(BlueprintAssignable) FRPGQuestOffersChanged OnOffersChanged;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) bool GenerateOffer();
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) void RefreshOffers();
	bool GetValidOffer(FGuid OfferId, FRPGQuestOffer& Out) const;
	void ConsumeOffer(FGuid OfferId);
private:
	FTimerHandle OfferTimer;
	FRandomStream QuestRandom;
	bool bRandomInitialized = false;
	bool bGenerating = false;
	bool bRefreshing = false;
	UFUNCTION() void OnRep_Offers();
};
