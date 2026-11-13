#pragma once

#include "Components/ActorComponent.h"
#include "RPGPartyComponent.generated.h"

class ARPGPlayerState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRPGPartyChanged);

/** Server-owned party membership. Each member receives the same replicated roster. */
UCLASS(ClassGroup=(Party), BlueprintType, meta=(BlueprintSpawnableComponent))
class DUTRAVELLER_API URPGPartyComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	URPGPartyComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Party) FGuid PartyId;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Party) TObjectPtr<ARPGPlayerState> Leader;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Party) TArray<TObjectPtr<ARPGPlayerState>> Members;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Invite) FGuid PendingPartyId;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Invite) TObjectPtr<ARPGPlayerState> PendingInviter;
	/** Replicated party mode. Supported capacities are 4, 10, and 20 players. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Party, Category="Party") int32 PartySizeLimit = 4;
	UPROPERTY(BlueprintAssignable) FRPGPartyChanged OnPartyChanged;
	UPROPERTY(BlueprintAssignable) FRPGPartyChanged OnInviteChanged;
	UFUNCTION(BlueprintCallable) void RequestInvite(ARPGPlayerState* Target);
	UFUNCTION(BlueprintCallable, Category="Party") void RequestSetPartySizeLimit(int32 NewLimit);
	UFUNCTION(BlueprintCallable) void AcceptInvite();
	UFUNCTION(BlueprintCallable) void DeclineInvite();
	UFUNCTION(BlueprintCallable) void LeaveParty();
	UFUNCTION(BlueprintPure) bool IsInParty() const { return PartyId.IsValid() && Members.Contains(Cast<ARPGPlayerState>(GetOwner())); }
private:
	UFUNCTION() void OnRep_Party();
	UFUNCTION() void OnRep_Invite();
	UFUNCTION(Server, Reliable) void ServerInvite(ARPGPlayerState* Target);
	UFUNCTION(Server, Reliable) void ServerSetPartySizeLimit(int32 NewLimit);
	UFUNCTION(Server, Reliable) void ServerAcceptInvite();
	UFUNCTION(Server, Reliable) void ServerDeclineInvite();
	UFUNCTION(Server, Reliable) void ServerLeaveParty();
	void InviteAuthority(ARPGPlayerState* Target);
	void SetPartySizeLimitAuthority(int32 NewLimit);
	void AcceptAuthority();
	void DeclineAuthority();
	void LeaveAuthority();
	static bool IsSupportedPartySize(int32 Limit);
	static void Synchronize(const TArray<ARPGPlayerState*>& NewMembers, ARPGPlayerState* NewLeader, const FGuid& NewPartyId, int32 NewLimit);
};
