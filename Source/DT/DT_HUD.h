// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DT_HUD.generated.h"

class UDTUIRoot;

/**
 * 
 */
UCLASS()
class DT_API ADT_HUD : public AHUD
{
	GENERATED_BODY()
public:
	/** HUD Blueprints access the same root that the owning controller manages. */
	UFUNCTION(BlueprintPure, Category="DT|UI")
	UDTUIRoot* GetPlayerUIRoot() const;
};
