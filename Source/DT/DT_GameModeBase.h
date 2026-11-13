// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DuTraveller/RPGGameModeBase.h"
#include "DT_GameModeBase.generated.h"

/**
 * Keeps the historical DT GameMode Blueprint path while inheriting coherent
 * RPG defaults for controller, player state and pawn when a map does not
 * override them.
 */
UCLASS(BlueprintType, Blueprintable)
class DT_API ADT_GameModeBase : public ARPGGameModeBase
{
	GENERATED_BODY()
	
};
