#include "RPGPartyComponent.h"

#include "Net/UnrealNetwork.h"
#include "RPGPlayerState.h"

URPGPartyComponent::URPGPartyComponent() { SetIsReplicatedByDefault(true); }

void URPGPartyComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(URPGPartyComponent, PartyId);
	DOREPLIFETIME(URPGPartyComponent, Leader);
	DOREPLIFETIME(URPGPartyComponent, Members);
	DOREPLIFETIME(URPGPartyComponent, PartySizeLimit);
	DOREPLIFETIME_CONDITION(URPGPartyComponent, PendingPartyId, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(URPGPartyComponent, PendingInviter, COND_OwnerOnly);
}

void URPGPartyComponent::OnRep_Party() { OnPartyChanged.Broadcast(); }
void URPGPartyComponent::OnRep_Invite() { OnInviteChanged.Broadcast(); }

void URPGPartyComponent::RequestInvite(ARPGPlayerState* Target) { if (GetOwner() && GetOwner()->HasAuthority()) InviteAuthority(Target); else ServerInvite(Target); }
void URPGPartyComponent::RequestSetPartySizeLimit(int32 NewLimit) { if (GetOwner() && GetOwner()->HasAuthority()) SetPartySizeLimitAuthority(NewLimit); else ServerSetPartySizeLimit(NewLimit); }
void URPGPartyComponent::AcceptInvite() { if (GetOwner() && GetOwner()->HasAuthority()) AcceptAuthority(); else ServerAcceptInvite(); }
void URPGPartyComponent::DeclineInvite() { if (GetOwner() && GetOwner()->HasAuthority()) DeclineAuthority(); else ServerDeclineInvite(); }
void URPGPartyComponent::LeaveParty() { if (GetOwner() && GetOwner()->HasAuthority()) LeaveAuthority(); else ServerLeaveParty(); }
void URPGPartyComponent::ServerInvite_Implementation(ARPGPlayerState* Target) { InviteAuthority(Target); }
void URPGPartyComponent::ServerSetPartySizeLimit_Implementation(int32 NewLimit) { SetPartySizeLimitAuthority(NewLimit); }
void URPGPartyComponent::ServerAcceptInvite_Implementation() { AcceptAuthority(); }
void URPGPartyComponent::ServerDeclineInvite_Implementation() { DeclineAuthority(); }
void URPGPartyComponent::ServerLeaveParty_Implementation() { LeaveAuthority(); }

void URPGPartyComponent::InviteAuthority(ARPGPlayerState* Target)
{
	ARPGPlayerState* Self = Cast<ARPGPlayerState>(GetOwner());
	URPGPartyComponent* Other = Target ? Target->Party : nullptr;
	if (!Self || !Self->HasAuthority() || !Other || Target == Self || Target->GetWorld() != Self->GetWorld() || Other->IsInParty()) return;
	TArray<ARPGPlayerState*> Roster;
	if (IsInParty())
	{
		if (Leader != Self || Members.Num() >= PartySizeLimit) return;
		for (ARPGPlayerState* Member : Members) if (IsValid(Member)) Roster.Add(Member);
	}
	else
	{
		Roster.Add(Self);
		Synchronize(Roster, Self, FGuid::NewGuid(), PartySizeLimit);
	}
	Other->PendingPartyId = PartyId;
	Other->PendingInviter = Self;
	Other->OnRep_Invite(); Target->ForceNetUpdate();
}

bool URPGPartyComponent::IsSupportedPartySize(int32 Limit)
{
	return Limit == 4 || Limit == 10 || Limit == 20;
}

void URPGPartyComponent::SetPartySizeLimitAuthority(int32 NewLimit)
{
	ARPGPlayerState* Self = Cast<ARPGPlayerState>(GetOwner());
	if (!Self || !Self->HasAuthority() || !IsSupportedPartySize(NewLimit)) return;
	if (!IsInParty())
	{
		PartySizeLimit = NewLimit;
		OnRep_Party();
		Self->ForceNetUpdate();
		return;
	}
	if (Leader != Self || Members.Num() > NewLimit) return;
	TArray<ARPGPlayerState*> Roster;
	for (ARPGPlayerState* Member : Members) if (IsValid(Member)) Roster.AddUnique(Member);
	Synchronize(Roster, Leader, PartyId, NewLimit);
}

void URPGPartyComponent::AcceptAuthority()
{
	ARPGPlayerState* Self = Cast<ARPGPlayerState>(GetOwner());
	URPGPartyComponent* Source = PendingInviter ? PendingInviter->Party : nullptr;
	if (!Self || !Self->HasAuthority() || IsInParty() || !PendingPartyId.IsValid() || !Source || Source->PartyId != PendingPartyId || Source->Leader != PendingInviter || Source->Members.Num() >= Source->PartySizeLimit) { DeclineAuthority(); return; }
	TArray<ARPGPlayerState*> Roster;
	for (ARPGPlayerState* Member : Source->Members) if (IsValid(Member)) Roster.AddUnique(Member);
	Roster.AddUnique(Self);
	const FGuid AcceptedId = PendingPartyId;
	ARPGPlayerState* AcceptedLeader = PendingInviter;
	const int32 AcceptedLimit = Source->PartySizeLimit;
	DeclineAuthority();
	Synchronize(Roster, AcceptedLeader, AcceptedId, AcceptedLimit);
}

void URPGPartyComponent::DeclineAuthority()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	PendingPartyId.Invalidate(); PendingInviter = nullptr; OnRep_Invite(); GetOwner()->ForceNetUpdate();
}

void URPGPartyComponent::LeaveAuthority()
{
	ARPGPlayerState* Self = Cast<ARPGPlayerState>(GetOwner());
	if (!Self || !Self->HasAuthority() || !IsInParty()) return;
	const FGuid OldPartyId = PartyId;
	ARPGPlayerState* OldLeader = Leader;
	const int32 OldLimit = PartySizeLimit;
	TArray<ARPGPlayerState*> Remaining;
	for (ARPGPlayerState* Member : Members) if (IsValid(Member) && Member != Self) Remaining.Add(Member);
	PartyId.Invalidate(); Leader = nullptr; Members.Reset(); PartySizeLimit = 4; OnRep_Party(); Self->ForceNetUpdate();
	if (Remaining.Num() < 2) Synchronize(Remaining, nullptr, FGuid(), 4);
	else Synchronize(Remaining, OldLeader == Self ? Remaining[0] : OldLeader, OldPartyId, OldLimit);
}

void URPGPartyComponent::Synchronize(const TArray<ARPGPlayerState*>& NewMembers, ARPGPlayerState* NewLeader, const FGuid& NewPartyId, int32 NewLimit)
{
	const bool bValidParty = NewPartyId.IsValid() && IsValid(NewLeader) && NewMembers.Num() >= 1;
	const int32 EffectiveLimit = bValidParty && IsSupportedPartySize(NewLimit) ? NewLimit : 4;
	for (ARPGPlayerState* Member : NewMembers)
	{
		if (!IsValid(Member) || !Member->HasAuthority() || !Member->Party) continue;
		Member->Party->PartyId = bValidParty ? NewPartyId : FGuid();
		Member->Party->Leader = bValidParty ? NewLeader : nullptr;
		Member->Party->PartySizeLimit = EffectiveLimit;
		Member->Party->Members.Reset();
		if (bValidParty) for (ARPGPlayerState* Entry : NewMembers) if (IsValid(Entry)) Member->Party->Members.AddUnique(Entry);
		Member->Party->OnRep_Party(); Member->ForceNetUpdate();
	}
}
