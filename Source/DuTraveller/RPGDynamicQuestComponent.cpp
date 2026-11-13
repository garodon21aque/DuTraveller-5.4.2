#include "RPGDynamicQuestComponent.h"
#include "RPGRandomQuestSpawner.h"
#include "RPGSpawnArea.h"
#include "RPGPlayerState.h"
#include "RPGCharacter.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "RPGPartyComponent.h"

URPGDynamicQuestComponent::URPGDynamicQuestComponent() { SetIsReplicatedByDefault(true); }
void URPGDynamicQuestComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(URPGDynamicQuestComponent, Records, COND_OwnerOnly);
}
ARPGPlayerState* URPGDynamicQuestComponent::OwningPlayer() const { return Cast<ARPGPlayerState>(GetOwner()); }
void URPGDynamicQuestComponent::OnRep_Quests() { OnQuestsChanged.Broadcast(); }
void URPGDynamicQuestComponent::ClientRequestResult_Implementation(const bool bSucceeded, const FName Operation)
{
	OnRequestResult.Broadcast(bSucceeded, Operation);
}
void URPGDynamicQuestComponent::RequestAcceptOffer(ARPGRandomQuestSpawner* Spawner, const FGuid OfferId)
{
	if (!GetOwner()) return;
	if (GetOwner()->HasAuthority()) ClientRequestResult(TryAcceptOffer(Spawner, OfferId), FName("Accept"));
	else ServerAcceptOffer(Spawner, OfferId);
}
void URPGDynamicQuestComponent::ServerAcceptOffer_Implementation(ARPGRandomQuestSpawner* Spawner, const FGuid OfferId)
{
	ClientRequestResult(TryAcceptOffer(Spawner, OfferId), FName("Accept"));
}
void URPGDynamicQuestComponent::RequestClaimReward(const FGuid InstanceId)
{
	if (!GetOwner()) return;
	if (GetOwner()->HasAuthority()) ClientRequestResult(TryClaimReward(InstanceId), FName("Claim"));
	else ServerClaimReward(InstanceId);
}
void URPGDynamicQuestComponent::ServerClaimReward_Implementation(const FGuid InstanceId)
{
	ClientRequestResult(TryClaimReward(InstanceId), FName("Claim"));
}

bool URPGDynamicQuestComponent::HasCompletedQuest(const FName QuestId) const
{
	return Records.ContainsByPredicate([&](const FRPGDynamicQuestRecord& Record) { return Record.QuestId == QuestId && Record.Status == ERPGDynamicQuestStatus::Claimed; });
}

bool URPGDynamicQuestComponent::Eligible(const URPGRandomQuestDefinition* Definition, const FName GroupId, const int32 GroupLimit) const
{
	ARPGPlayerState* PS = OwningPlayer();
	if (!PS || !Definition || !Definition->IsConfigured() || PS->LVL < Definition->MinimumLevel ||
		(Definition->MaximumLevel > 0 && PS->LVL > Definition->MaximumLevel) || GroupLimit < 1 || GroupId.IsNone() ||
		!PS->AbilitySystem->HasAllMatchingGameplayTags(Definition->RequiredTags) ||
		MaximumHistory < 1 || MaximumHistory > 4096 || Records.Num() >= MaximumHistory || MaximumActive < 1 || MaximumActive > 64) return false;
	for (FName Prerequisite : Definition->PrerequisiteQuests) if (Prerequisite.IsNone() || !HasCompletedQuest(Prerequisite)) return false;
	int32 Active = 0, ActiveInGroup = 0;
	for (const auto& Record : Records)
	{
		if (Record.QuestId == Definition->QuestId && (Record.Status != ERPGDynamicQuestStatus::Claimed || !Definition->bRepeatable)) return false;
		if (Record.Status == ERPGDynamicQuestStatus::Claimed) continue;
		++Active;
		if (Record.GroupId == GroupId) ++ActiveInGroup;
	}
	return Active < MaximumActive && ActiveInGroup < GroupLimit;
}

bool URPGDynamicQuestComponent::TryAcceptOffer(ARPGRandomQuestSpawner* Spawner, const FGuid OfferId)
{
	ARPGPlayerState* PS = OwningPlayer();
	ARPGCharacter* Pawn = PS ? PS->GetPawn<ARPGCharacter>() : nullptr;
	if (!PS || !PS->HasAuthority() || bMutating || !Pawn || !IsValid(Spawner) || Spawner->GetWorld() != GetWorld() ||
		!Spawner->ContainsPosition(Pawn->GetActorLocation()) || Records.ContainsByPredicate([&](const FRPGDynamicQuestRecord& Record) { return Record.SourceOfferId == OfferId; })) return false;
	FRPGQuestOffer Offer;
	if (!Spawner->GetValidOffer(OfferId, Offer) || !Eligible(Offer.Definition, Offer.Group->GroupId, Offer.Group->MaximumActivePerPlayer) ||
		(!Offer.Definition->AllowedLocations.IsEmpty() && !Offer.Definition->AllowedLocations.Contains(Spawner->LocationId))) return false;
	TGuardValue<bool> Guard(bMutating, true);
	FRPGDynamicQuestRecord Record;
	Record.InstanceId = FGuid::NewGuid(); Record.SourceOfferId = OfferId;
	Record.Definition = Offer.Definition; Record.QuestId = Offer.Definition->QuestId;
	Record.GroupId = Offer.Group->GroupId; Record.LocationId = Spawner->LocationId;
	Record.Progress.Init(0, Offer.Definition->Objectives.Num());
	Record.ExperienceReward = Offer.Definition->ExperienceReward;
	FRandomStream Random(static_cast<int32>(GetTypeHash(Record.InstanceId)));
	if (!Offer.Definition->RollRewards(Random, Record.RolledRewards)) return false;
	TArray<URPGDynamicQuestComponent*> SharedRecipients;
	if (Offer.Definition->Audience == ERPGQuestAudience::Party)
	{
		if (!PS->Party || !PS->Party->IsInParty() || PS->Party->Members.Num() < 2) return false;
		for (ARPGPlayerState* Member : PS->Party->Members)
		{
			if (!IsValid(Member) || !Member->DynamicQuests ||
				!Member->DynamicQuests->Eligible(Offer.Definition, Offer.Group->GroupId, Offer.Group->MaximumActivePerPlayer)) return false;
			if (Member != PS) SharedRecipients.Add(Member->DynamicQuests);
		}
	}
	Records.Add(Record);
	for (URPGDynamicQuestComponent* Recipient : SharedRecipients)
		if (!Recipient->AddSharedRecord(Record)) { Records.Pop(); return false; }
	bInitialized = true;
	Spawner->ConsumeOffer(OfferId);
	OnRep_Quests();
	OnQuestAccepted.Broadcast(Record.InstanceId, Spawner);
	PS->ForceNetUpdate();
	return true;
}

bool URPGDynamicQuestComponent::RecordObjectiveEvent(const FName Event, AActor* Subject, const int32 Amount)
{
	ARPGPlayerState* PS = OwningPlayer();
	if (!PS || !PS->HasAuthority() || bMutating || Event.IsNone() || Amount <= 0 || (Subject && (!IsValid(Subject) || Subject->GetWorld() != GetWorld()))) return false;
	const bool bPartyQuest = Records.ContainsByPredicate([&](const FRPGDynamicQuestRecord& Record)
	{
		return Record.Status == ERPGDynamicQuestStatus::Active && IsValid(Record.Definition) &&
			Record.Definition->Audience == ERPGQuestAudience::Party;
	});
	if (bPartyQuest && PS->Party && PS->Party->IsInParty())
	{
		bool bAny = false;
		for (ARPGPlayerState* Member : PS->Party->Members)
			if (IsValid(Member) && Member->DynamicQuests) bAny |= Member->DynamicQuests->RecordObjectiveEventInternal(Event, Subject, Amount, true);
		return bAny;
	}
	return RecordObjectiveEventInternal(Event, Subject, Amount);
}

bool URPGDynamicQuestComponent::RecordObjectiveEventInternal(const FName Event, AActor* Subject, const int32 Amount, const bool bOnlyPartyQuests)
{
	ARPGPlayerState* PS = OwningPlayer();
	if (!PS || !PS->HasAuthority() || bMutating) return false;
	TGuardValue<bool> Guard(bMutating, true);
	const auto* Context = Subject ? Subject->FindComponentByClass<URPGSpawnContextComponent>() : nullptr;
	bool bChanged = false;
	for (auto& Record : Records)
	{
		if (Record.Status != ERPGDynamicQuestStatus::Active || !IsValid(Record.Definition) || Record.Progress.Num() != Record.Definition->Objectives.Num()) continue;
		if (bOnlyPartyQuests && Record.Definition->Audience != ERPGQuestAudience::Party) continue;
		for (int32 Index = 0; Index < Record.Progress.Num(); ++Index)
		{
			const auto& Objective = Record.Definition->Objectives[Index];
			if (Objective.Event != Event || (Objective.TargetClass && (!Subject || !Subject->IsA(Objective.TargetClass))) ||
				(Objective.bRequireQuestActor && (!Context || !Context->SpawnId.IsValid() || Context->QuestInstanceId != Record.InstanceId || Context->LocationId != Record.LocationId))) continue;
			if (Context && Record.Credits.ContainsByPredicate([&](const FRPGObjectiveCredit& Credit) { return Credit.SpawnId == Context->SpawnId && Credit.ObjectiveId == Objective.ObjectiveId; })) continue;
			const int32 Next = static_cast<int32>(FMath::Min<int64>(Objective.RequiredCount, static_cast<int64>(Record.Progress[Index]) + Amount));
			if (Context && Next != Record.Progress[Index])
			{
				FRPGObjectiveCredit Credit; Credit.SpawnId = Context->SpawnId; Credit.ObjectiveId = Objective.ObjectiveId;
				Record.Credits.Add(Credit);
			}
			bChanged |= Next != Record.Progress[Index];
			Record.Progress[Index] = Next;
		}
		bool bComplete = true;
		for (int32 Index = 0; Index < Record.Progress.Num(); ++Index)
			if (!Record.Definition->Objectives[Index].bOptional && Record.Progress[Index] < Record.Definition->Objectives[Index].RequiredCount) bComplete = false;
		if (bComplete) { Record.Status = ERPGDynamicQuestStatus::Complete; bChanged = true; }
	}
	if (bChanged) { OnRep_Quests(); PS->ForceNetUpdate(); }
	return bChanged;
}

bool URPGDynamicQuestComponent::AddSharedRecord(const FRPGDynamicQuestRecord& Record)
{
	ARPGPlayerState* PS = OwningPlayer();
	if (!PS || !PS->HasAuthority() || bMutating || !IsValid(Record.Definition) ||
		Record.Definition->Audience != ERPGQuestAudience::Party || Records.Num() >= MaximumHistory ||
		Records.ContainsByPredicate([&](const FRPGDynamicQuestRecord& Existing) { return Existing.InstanceId == Record.InstanceId; })) return false;
	Records.Add(Record); bInitialized = true; OnRep_Quests(); PS->ForceNetUpdate();
	return true;
}

bool URPGDynamicQuestComponent::TryClaimReward(const FGuid InstanceId)
{
	ARPGPlayerState* PS = OwningPlayer();
	ARPGCharacter* Pawn = PS ? PS->GetPawn<ARPGCharacter>() : nullptr;
	if (!PS || !PS->HasAuthority() || !Pawn || bMutating) return false;
	FRPGDynamicQuestRecord* Record = Records.FindByPredicate([&](const FRPGDynamicQuestRecord& Entry) { return Entry.InstanceId == InstanceId; });
	if (!Record || Record->Status != ERPGDynamicQuestStatus::Complete || !FMath::IsFinite(Record->ExperienceReward) || Record->ExperienceReward < 0 ||
		!FMath::IsFinite(PS->EXP) || !FMath::IsFinite(PS->EXP + Record->ExperienceReward)) return false;
	TGuardValue<bool> Guard(bMutating, true);
	if (!Pawn->InventoryComp->TryExchangeItems({}, Record->RolledRewards)) return false;
	Record->Status = ERPGDynamicQuestStatus::Claimed;
	if (!PS->GrantExperience(Record->ExperienceReward, false)) return false;
	OnRep_Quests(); PS->ForceNetUpdate();
	return true;
}

FRPGDynamicQuestSnapshot URPGDynamicQuestComponent::CaptureQuestState() const
{
	FRPGDynamicQuestSnapshot Snapshot;
	Snapshot.Records = Records;
	return Snapshot;
}

bool URPGDynamicQuestComponent::RestoreQuestState(const FRPGDynamicQuestSnapshot& Snapshot)
{
	ARPGPlayerState* PS = OwningPlayer();
	if (!PS || !PS->HasAuthority() || bMutating || bInitialized || !Records.IsEmpty() || Snapshot.SchemaVersion != 1 ||
		MaximumHistory < 1 || MaximumHistory > 4096 || Snapshot.Records.Num() > MaximumHistory) return false;
	TSet<FGuid> Instances, Offers;
	for (const auto& Record : Snapshot.Records)
	{
		if (!Record.InstanceId.IsValid() || !Record.SourceOfferId.IsValid() || Instances.Contains(Record.InstanceId) || Offers.Contains(Record.SourceOfferId) ||
			!IsValid(Record.Definition) || !Record.Definition->IsConfigured() || Record.QuestId != Record.Definition->QuestId || Record.GroupId.IsNone() || Record.LocationId.IsNone() ||
			Record.Progress.Num() != Record.Definition->Objectives.Num() || !FMath::IsFinite(Record.ExperienceReward) || Record.ExperienceReward < 0 ||
			static_cast<uint8>(Record.Status) > static_cast<uint8>(ERPGDynamicQuestStatus::Claimed) || Record.RolledRewards.Num() > 160 || Record.Credits.Num() > 4096) return false;
		for (int32 Index = 0; Index < Record.Progress.Num(); ++Index)
		{
			const auto& Objective = Record.Definition->Objectives[Index];
			if (Record.Progress[Index] < 0 || Record.Progress[Index] > Objective.RequiredCount ||
				(Record.Status != ERPGDynamicQuestStatus::Active && !Objective.bOptional && Record.Progress[Index] < Objective.RequiredCount)) return false;
		}
		for (const auto& Reward : Record.RolledRewards)
			if (!Reward.ItemClass || Reward.ItemClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated) || Reward.Quantity <= 0) return false;
		TSet<FString> CreditKeys;
		for (const auto& Credit : Record.Credits)
		{
			const FString Key = Credit.SpawnId.ToString() + TEXT(":") + Credit.ObjectiveId.ToString();
			if (!Credit.SpawnId.IsValid() || CreditKeys.Contains(Key) || !Record.Definition->Objectives.ContainsByPredicate(
				[&](const FRPGDynamicObjective& Objective) { return Objective.ObjectiveId == Credit.ObjectiveId; })) return false;
			CreditKeys.Add(Key);
		}
		Instances.Add(Record.InstanceId); Offers.Add(Record.SourceOfferId);
	}
	TGuardValue<bool> Guard(bMutating, true);
	Records = Snapshot.Records; bInitialized = true;
	OnRep_Quests(); PS->ForceNetUpdate();
	return true;
}
