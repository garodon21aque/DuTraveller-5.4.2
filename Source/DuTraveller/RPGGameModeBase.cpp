#include "RPGGameModeBase.h"
#include "RPGCharacter.h"
#include "RPGPlayerController.h"
#include "RPGPlayerState.h"
#include "TimerManager.h"
#include "Engine/World.h"

ARPGGameModeBase::ARPGGameModeBase()
{
	DefaultPawnClass = ARPGCharacter::StaticClass();
	PlayerControllerClass = ARPGPlayerController::StaticClass();
	PlayerStateClass = ARPGPlayerState::StaticClass();
}

void ARPGGameModeBase::HandlePlayerDeath(ARPGCharacter* Player)
{
	ARPGPlayerController* PC = Player ? Cast<ARPGPlayerController>(Player->GetController()) : nullptr;
	if (!HasAuthority() || !PC || !Player->bIsDead) return;
	const float Delay = FMath::Max(1.f, PlayerRespawnDelay);
	PC->ClientShowDeath(Delay);
	TWeakObjectPtr<ARPGCharacter> Victim = Player;
	TWeakObjectPtr<ARPGPlayerController> Controller = PC;
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Victim, Controller]()
	{
		if (!Victim.IsValid() || !Controller.IsValid() || Controller->GetPawn() != Victim.Get() || !Victim->bIsDead) return;
		AActor* Start = FindPlayerStart(Controller.Get());
		if (Start) { Victim->RespawnAt(Start->GetActorTransform()); Controller->SetControlRotation(Start->GetActorRotation()); }
	}), Delay, false);
}
