#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RPGSkillExecution.generated.h"
class ARPGCharacter;
class URPGCombatAbilityDefinition;

UCLASS()
class DUTRAVELLER_API URPGSkillExecution : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	static void Execute(ARPGCharacter* Source, const URPGCombatAbilityDefinition* Definition, ARPGCharacter* Target, float Damage, int32 Rank);
	static void RestoreResources(ARPGCharacter* Character, float Health, float Mana);
};
