#include "RPGRandomQuestSpawner.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

ARPGRandomQuestSpawner::ARPGRandomQuestSpawner() { bAutoSpawn = false; }
void ARPGRandomQuestSpawner::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && bGenerateAutomatically && FMath::IsFinite(RefreshInterval) && RefreshInterval >= 0.5f)
	{
		RefreshOffers();
		GetWorldTimerManager().SetTimer(OfferTimer, this, &ARPGRandomQuestSpawner::RefreshOffers, RefreshInterval, true);
	}
}
void ARPGRandomQuestSpawner::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(OfferTimer);
	Super::EndPlay(Reason);
}
void ARPGRandomQuestSpawner::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARPGRandomQuestSpawner, Offers);
}
void ARPGRandomQuestSpawner::OnRep_Offers() { OnOffersChanged.Broadcast(); }

bool ARPGRandomQuestSpawner::GenerateOffer()
{
	if (!HasAuthority() || bGenerating || AreaId.IsNone() || LocationId.IsNone() || MaximumOffers < 1 || MaximumOffers > 32 ||
		Offers.Num() >= MaximumOffers || !FMath::IsFinite(OfferLifetime) || OfferLifetime < 1.f || QuestGroups.Num() > 128) return false;
	struct FCandidate { URPGQuestGroupDefinition* Group; URPGRandomQuestDefinition* Quest; int64 Weight; };
	TArray<FCandidate> Candidates;
	int64 Total = 0;
	TSet<FName> Seen;
	for (URPGQuestGroupDefinition* Group : QuestGroups)
	{
		if (!IsValid(Group) || Group->GroupId.IsNone() || Group->Weight < 1 || Group->Quests.Num() > 256 || Group->MaximumActivePerPlayer < 1) continue;
		const TArray<FRPGWeightedQuest> Entries = Group->GetQuestEntries();
		if (Entries.Num() > 256) continue;
		for (const auto& Entry : Entries)
		{
			URPGRandomQuestDefinition* Quest = Entry.Definition;
			if (!IsValid(Quest) || !Quest->IsConfigured() || Entry.Weight < 1 || Seen.Contains(Quest->QuestId) ||
				(!Quest->AllowedLocations.IsEmpty() && !Quest->AllowedLocations.Contains(LocationId)) ||
				Offers.ContainsByPredicate([&](const FRPGQuestOffer& Offer) { return Offer.Definition && Offer.Definition->QuestId == Quest->QuestId; })) continue;
			const int64 Weight = static_cast<int64>(Group->Weight) * Entry.Weight;
			if (Weight > MAX_int32 || Total + Weight > MAX_int32) return false;
			Candidates.Add({Group, Quest, Weight}); Total += Weight; Seen.Add(Quest->QuestId);
		}
	}
	if (Candidates.IsEmpty()) return false;
	TGuardValue<bool> Guard(bGenerating, true);
	if (!bRandomInitialized) { QuestRandom.Initialize(RandomSeed); bRandomInitialized = true; }
	int64 Pick = QuestRandom.RandRange(1, static_cast<int32>(Total));
	for (const auto& Candidate : Candidates)
	{
		Pick -= Candidate.Weight;
		if (Pick > 0) continue;
		FRPGQuestOffer Offer;
		Offer.OfferId = FGuid::NewGuid(); Offer.Definition = Candidate.Quest; Offer.Group = Candidate.Group;
		Offer.ExpiresAt = GetWorld()->GetTimeSeconds() + OfferLifetime;
		Offers.Add(Offer);
		OnRep_Offers(); ForceNetUpdate();
		return true;
	}
	return false;
}

void ARPGRandomQuestSpawner::RefreshOffers()
{
	if (!HasAuthority() || bGenerating || bRefreshing) return;
	TGuardValue<bool> Guard(bRefreshing, true);
	const int32 Removed = Offers.RemoveAll([&](const FRPGQuestOffer& Offer) { return Offer.ExpiresAt <= GetWorld()->GetTimeSeconds() || !IsValid(Offer.Definition); });
	if (Removed) { OnRep_Offers(); ForceNetUpdate(); }
	for (int32 Attempt = 0; Attempt < FMath::Clamp(MaximumOffers, 0, 32); ++Attempt) if (!GenerateOffer()) break;
}

bool ARPGRandomQuestSpawner::GetValidOffer(const FGuid OfferId, FRPGQuestOffer& Out) const
{
	if (!HasAuthority() || !OfferId.IsValid()) return false;
	for (const auto& Offer : Offers)
	{
		if (Offer.OfferId != OfferId || !IsValid(Offer.Definition) || !IsValid(Offer.Group) ||
			Offer.ExpiresAt <= GetWorld()->GetTimeSeconds()) continue;
		Out = Offer; return true;
	}
	return false;
}

void ARPGRandomQuestSpawner::ConsumeOffer(const FGuid OfferId)
{
	if (!HasAuthority() || !bSingleConsumerOffers) return;
	if (Offers.RemoveAll([&](const FRPGQuestOffer& Offer) { return Offer.OfferId == OfferId; })) { OnRep_Offers(); ForceNetUpdate(); }
}
