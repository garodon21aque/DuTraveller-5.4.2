// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DuTraveller/RPGPlayerState.h"
#include "DT_PlayerState.generated.h"

/** Modern framework identity backed by the authoritative RPG/GAS player state. */
UCLASS(BlueprintType, Blueprintable)
class DT_API ADT_PlayerState : public ARPGPlayerState
{
	GENERATED_BODY()
	
};
