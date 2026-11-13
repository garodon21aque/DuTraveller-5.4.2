#include "RPGQuest.h"
#include "RPGCharacter.h"
#include "RPGPlayerState.h"
#include "RPGDynamicQuestComponent.h"
#include "RPGRandomQuestSpawner.h"
#include "EngineUtils.h"

URPGQuestComponent::URPGQuestComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URPGQuest::IsAllObjectiveComplete(bool& Complete)
{
	Complete = true;
	for (const FStageDetails& StageDetails : QuestDetails.Stages)
	{
		for (const FObjectiveDetails& Objective : StageDetails.Objectives)
		{
			if (!Objective.IsComplete)
			{
				Complete = false;
				return;
			}
		}
	}
	for (const FObjectiveDetails& Objective : QuestDetails.Objectives)
	{
		if (!Objective.IsComplete)
		{
			Complete = false;
			return;
		}
	}
}

void URPGQuestComponent::AddNewQuest(FName QuestID, UDataTable* DataTable)
{
	const ARPGCharacter* Character = Cast<ARPGCharacter>(GetOwner());
	ARPGPlayerState* PS = Character ? Character->GetPlayerState<ARPGPlayerState>() : nullptr;
	if (!PS || QuestID.IsNone()) return;
	// Legacy calls resolve only published authoritative offers; a client table
	// cannot inject a definition or its rewards into the server's quest state.
	for (TActorIterator<ARPGRandomQuestSpawner> It(GetWorld()); It; ++It)
	{
		for (const auto& Offer : It->Offers)
		{
			if (Offer.Definition && Offer.Definition->QuestId == QuestID && It->ContainsPosition(Character->GetActorLocation()))
			{ PS->DynamicQuests->RequestAcceptOffer(*It, Offer.OfferId); return; }
		}
	}
}

void URPGQuestComponent::TurnInQuest(FName QuestID)
{
	const ARPGCharacter* Character = Cast<ARPGCharacter>(GetOwner());
	ARPGPlayerState* PS = Character ? Character->GetPlayerState<ARPGPlayerState>() : nullptr;
	if (!PS) return;
	for (const auto& Record : PS->DynamicQuests->Records)
		if (Record.QuestId == QuestID && Record.Status == ERPGDynamicQuestStatus::Complete)
		{ PS->DynamicQuests->RequestClaimReward(Record.InstanceId); return; }
}
