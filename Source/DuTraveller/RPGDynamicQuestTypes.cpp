#include "RPGDynamicQuestTypes.h"

TArray<FRPGWeightedQuest> URPGQuestGroupDefinition::GetQuestEntries() const
{
	TArray<FRPGWeightedQuest> Result;
	TSet<FName> Seen;
	const auto Add = [&](URPGRandomQuestDefinition* Definition, int32 Weight)
	{
		if (!IsValid(Definition) || !Definition->IsConfigured() || Weight < 1 || Seen.Contains(Definition->QuestId)) return;
		FRPGWeightedQuest Entry; Entry.Definition = Definition; Entry.Weight = Weight;
		Result.Add(Entry); Seen.Add(Definition->QuestId);
	};
	if (QuestCatalogue)
	{
		if (QuestCatalogue->GetRowStruct() != FRPGQuestCatalogueRow::StaticStruct() || QuestCatalogue->GetRowMap().Num() > 256)
		{
			UE_LOG(LogTemp, Error, TEXT("Quest catalogue has incompatible row type or excessive rows: %s"), *QuestCatalogue->GetPathName());
			return Result;
		}
		TArray<FName> Names = QuestCatalogue->GetRowNames(); Names.Sort(FNameLexicalLess());
		for (FName Name : Names)
		{
			const FRPGQuestCatalogueRow* Row = QuestCatalogue->FindRow<FRPGQuestCatalogueRow>(Name, TEXT("Quest catalogue"));
			if (Row && Row->Definition && Row->Definition->QuestId == Name) Add(Row->Definition, Row->Weight);
		}
	}
	if (Quests.Num() <= 256) for (const FRPGWeightedQuest& Entry : Quests) Add(Entry.Definition, Entry.Weight);
	return Result;
}

bool URPGRandomQuestDefinition::IsConfigured() const
{
	if (QuestId.IsNone() || Objectives.IsEmpty() || Objectives.Num() > 64 || MinimumLevel < 1 ||
		(MaximumLevel != 0 && MaximumLevel < MinimumLevel) || RandomRewardRolls < 0 || RandomRewardRolls > 32 ||
		!FMath::IsFinite(ExperienceReward) || ExperienceReward < 0.f || GuaranteedRewards.Num() > 128 || RandomRewards.Num() > 128) return false;
	TSet<FName> Ids;
	bool bHasRequired = false;
	int64 TotalObjectives = 0;
	for (const auto& Objective : Objectives)
	{
		if (Objective.ObjectiveId.IsNone() || Objective.Event.IsNone() || Objective.RequiredCount < 1 || Ids.Contains(Objective.ObjectiveId)) return false;
		Ids.Add(Objective.ObjectiveId);
		TotalObjectives += Objective.RequiredCount;
		bHasRequired |= !Objective.bOptional;
	}
	for (const auto& Reward : GuaranteedRewards)
		if (!Reward.ItemClass || Reward.ItemClass->HasAnyClassFlags(CLASS_Abstract) || Reward.Quantity < 1) return false;
	int64 TotalWeight = 0;
	for (const auto& Reward : RandomRewards)
	{
		if (!Reward.ItemClass || Reward.ItemClass->HasAnyClassFlags(CLASS_Abstract) || Reward.Weight < 1 ||
			Reward.MinimumQuantity < 1 || Reward.MaximumQuantity < Reward.MinimumQuantity) return false;
		TotalWeight += Reward.Weight;
	}
	return bHasRequired && TotalObjectives <= 4096 && TotalWeight <= MAX_int32 &&
		(RandomRewardRolls == 0 || !RandomRewards.IsEmpty()) &&
		(!bRewardsWithoutReplacement || RandomRewardRolls <= RandomRewards.Num());
}

bool URPGRandomQuestDefinition::RollRewards(FRandomStream& Random, TArray<FRPGInventoryAmount>& Out) const
{
	if (!IsConfigured()) return false;
	TArray<FRPGInventoryAmount> Result = GuaranteedRewards;
	TArray<FRPGRandomRewardEntry> Pool = RandomRewards;
	for (int32 Roll = 0; Roll < RandomRewardRolls; ++Roll)
	{
		int32 Weight = 0;
		for (const auto& Entry : Pool) Weight += Entry.Weight;
		int32 Pick = Random.RandRange(1, Weight);
		for (int32 Index = 0; Index < Pool.Num(); ++Index)
		{
			Pick -= Pool[Index].Weight;
			if (Pick > 0) continue;
			FRPGInventoryAmount Reward;
			Reward.ItemClass = Pool[Index].ItemClass;
			Reward.Quantity = Random.RandRange(Pool[Index].MinimumQuantity, Pool[Index].MaximumQuantity);
			Result.Add(Reward);
			if (bRewardsWithoutReplacement) Pool.RemoveAt(Index);
			break;
		}
	}
	Out = MoveTemp(Result);
	return true;
}
