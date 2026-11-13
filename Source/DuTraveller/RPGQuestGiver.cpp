#include "RPGQuestGiver.h"

ARPGQuestGiver::ARPGQuestGiver()
{
	NPCRole = ERPGNPCRole::QuestGiver;
	FactionId = FName("Neutral");
}

void ARPGQuestGiver::IsAllObjectiveComplete(bool& Complete)
{
	Complete = false;
}
