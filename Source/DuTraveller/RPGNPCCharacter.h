#pragma once

#include "RPGCharacter.h"
#include "RPGNPCCharacter.generated.h"

UENUM(BlueprintType)
enum class ERPGNPCRole : uint8
{
	Generic,
	Enemy,
	QuestGiver,
	Mount
};

/** NPC branch shared by enemies, quest givers, mounts, and future NPC roles. */
UCLASS(BlueprintType, Blueprintable)
class DUTRAVELLER_API ARPGNPCCharacter : public ARPGCharacter
{
	GENERATED_BODY()

public:
	ARPGNPCCharacter();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="NPC")
	ERPGNPCRole NPCRole = ERPGNPCRole::Generic;
};
