// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DuTraveller/RPGPlayerCharacter.h"
#include "DT_PlayerCharacter.generated.h"

/**
 * Unified player-character base. Existing DT Blueprints keep their Motion
 * Matching, Pose Search and traversal graphs, while inheriting the restored
 * RPG inventory, equipment, quest, action and GAS actor-info lifecycle.
 */
UCLASS(BlueprintType, Blueprintable)
class DT_API ADT_PlayerCharacter : public ARPGPlayerCharacter
{
	GENERATED_BODY()
	
};
