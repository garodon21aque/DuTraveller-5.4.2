#include "RPGAction.h"

#include "RPGActionComponent.h"

UWorld* URPGAction::GetWorld() const { return OwningComponent ? OwningComponent->GetWorld() : nullptr; }
bool URPGAction::CanStart_Implementation(AActor* Instigator) { return IsValid(Instigator) && !bIsRunning; }
void URPGAction::StartAction_Implementation(AActor* Instigator)
{
	bIsRunning = true;
	OnActionStateChanged.Broadcast(this);
	// The base action completes immediately. Blueprint actions may override this
	// to keep running until their animation/projectile calls StopAction.
	StopAction(Instigator);
}
void URPGAction::StopAction_Implementation(AActor* Instigator)
{
	bIsRunning = false;
	OnActionStopped.Broadcast(this);
}

float URPGActionEffect::GetTimeRemaining() const
{
	if (!bStarted || Duration <= 0.f)
	{
		return 0.f;
	}

	const UWorld* World = GetWorld();
	const float Elapsed = World ? static_cast<float>(World->GetTimeSeconds()) - StartTime : 0.f;
	return FMath::Max(0.f, Duration - Elapsed);
}
