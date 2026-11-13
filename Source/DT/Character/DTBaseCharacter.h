// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DuTraveller/RPGCharacter.h"
#include "DTBaseCharacter.generated.h"

/**
 * Compatibility alias retained for older DT assets. New active player and NPC
 * classes branch directly from ARPGPlayerCharacter and ARPGNPCCharacter.
 */
UCLASS(BlueprintType, Blueprintable, meta=(DeprecatedNode, DeprecationMessage="Use RPGPlayerCharacter or RPGNPCCharacter branches"))
class DT_API ADTBaseCharacter : public ARPGCharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ADTBaseCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

};
