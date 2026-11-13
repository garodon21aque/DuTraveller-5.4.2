#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RPGDynamicQuestTypes.h"
#include "RPGDynamicQuestComponent.generated.h"

class ARPGRandomQuestSpawner;
class ARPGPlayerState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRPGDynamicQuestsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRPGDynamicQuestAccepted, FGuid, InstanceId, ARPGRandomQuestSpawner*, Spawner);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRPGQuestRequestResult, bool, bSucceeded, FName, Operation);

/** Lives on PlayerState so quest ownership outlives pawn replacement. */
UCLASS(ClassGroup=(Quests), BlueprintType, meta=(BlueprintSpawnableComponent))
class DUTRAVELLER_API URPGDynamicQuestComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	URPGDynamicQuestComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Quests) TArray<FRPGDynamicQuestRecord> Records;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="1", ClampMax="4096")) int32 MaximumHistory = 1024;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="1", ClampMax="64")) int32 MaximumActive = 20;
	UPROPERTY(BlueprintAssignable) FRPGDynamicQuestsChanged OnQuestsChanged;
	UPROPERTY(BlueprintAssignable) FRPGDynamicQuestAccepted OnQuestAccepted;
	UPROPERTY(BlueprintAssignable) FRPGQuestRequestResult OnRequestResult;
	UFUNCTION(BlueprintCallable) void RequestAcceptOffer(ARPGRandomQuestSpawner* Spawner, FGuid OfferId);
	UFUNCTION(BlueprintCallable) void RequestClaimReward(FGuid InstanceId);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) bool TryAcceptOffer(ARPGRandomQuestSpawner* Spawner, FGuid OfferId);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) bool TryClaimReward(FGuid InstanceId);
	/** Called by authoritative combat/interactions/crafting, never by a client progress RPC. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) bool RecordObjectiveEvent(FName Event, AActor* Subject, int32 Amount = 1);
	UFUNCTION(BlueprintPure) FRPGDynamicQuestSnapshot CaptureQuestState() const;
	/** Restore once into fresh server state; does not grant rewards or modify inventory. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) bool RestoreQuestState(const FRPGDynamicQuestSnapshot& Snapshot);
	UFUNCTION(BlueprintPure) bool HasCompletedQuest(FName QuestId) const;
private:
	bool bMutating = false;
	bool bInitialized = false;
	ARPGPlayerState* OwningPlayer() const;
	bool Eligible(const URPGRandomQuestDefinition* Definition, FName GroupId, int32 GroupLimit) const;
	bool AddSharedRecord(const FRPGDynamicQuestRecord& Record);
	bool RecordObjectiveEventInternal(FName Event, AActor* Subject, int32 Amount, bool bOnlyPartyQuests = false);
	UFUNCTION() void OnRep_Quests();
	UFUNCTION(Server, Reliable) void ServerAcceptOffer(ARPGRandomQuestSpawner* Spawner, FGuid OfferId);
	UFUNCTION(Server, Reliable) void ServerClaimReward(FGuid InstanceId);
	UFUNCTION(Client, Reliable) void ClientRequestResult(bool bSucceeded, FName Operation);
};
