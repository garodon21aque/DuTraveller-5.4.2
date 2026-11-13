#pragma once

#include "GameFramework/GameModeBase.h"
#include "RPGGameModeBase.generated.h"

/** Coherent RPG multiplayer framework. Existing maps are not silently redirected to this mode. */
UCLASS(Blueprintable)
class DUTRAVELLER_API ARPGGameModeBase : public AGameModeBase
{
	GENERATED_BODY()
public:
	ARPGGameModeBase();
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Respawn", meta=(ClampMin="1")) float PlayerRespawnDelay = 15.f;
	void HandlePlayerDeath(class ARPGCharacter* Player);
};
