#include "RPGAIController.h"

#include "EngineUtils.h"
#include "RPGActionComponent.h"
#include "RPGAbilitySystemComponent.h"
#include "RPGCharacter.h"
#include "RPGCombatTypes.h"
#include "Navigation/PathFollowingComponent.h"

ARPGAIController::ARPGAIController() { PrimaryActorTick.bCanEverTick = true; }

void ARPGAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	CombatTarget = nullptr;
	DecisionCountdown = 0.f;
}

void ARPGAIController::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority()) return;
	if (bDirectChase && IsValid(CombatTarget))
		if (ARPGCharacter* Self = Cast<ARPGCharacter>(GetPawn()))
			if (!Self->bIsDead && !CombatTarget->bIsDead && LineOfSightTo(CombatTarget))
				Self->AddMovementInput((CombatTarget->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal2D());
	DecisionCountdown -= DeltaSeconds;
	if (DecisionCountdown <= 0.f)
	{
		DecisionCountdown = FMath::Max(0.05f, DecisionInterval);
		RunCombatDecision();
	}
}

ARPGCharacter* ARPGAIController::AcquireCombatTarget()
{
	ARPGCharacter* Self = Cast<ARPGCharacter>(GetPawn());
	if (!HasAuthority() || !Self || Self->bIsDead || !GetWorld()) return nullptr;
	ARPGCharacter* Best = nullptr;
	float BestDistance = FMath::Square(DetectionRange);
	for (TActorIterator<ARPGCharacter> It(GetWorld()); It; ++It)
	{
		ARPGCharacter* Candidate = *It;
		if (!IsValid(Candidate) || Candidate == Self || Candidate->bIsDead || Candidate->FactionId == TEXT("Neutral") ||
			(bTargetPlayersOnly && !Candidate->IsPlayerControlled()) || !LineOfSightTo(Candidate) ||
			(!Self->FactionId.IsNone() && Candidate->FactionId == Self->FactionId)) continue;
		const float Distance = FVector::DistSquared(Self->GetActorLocation(), Candidate->GetActorLocation());
		if (Distance <= BestDistance) { BestDistance = Distance; Best = Candidate; }
	}
	CombatTarget = Best;
	return Best;
}

bool ARPGAIController::RunCombatDecision()
{
	ARPGCharacter* Self = Cast<ARPGCharacter>(GetPawn());
	if (!HasAuthority() || !Self || Self->bIsDead || !Self->ActionComp) { bDirectChase = false; StopMovement(); return false; }
	if (!IsValid(CombatTarget) || CombatTarget->bIsDead || FVector::DistSquared(Self->GetActorLocation(), CombatTarget->GetActorLocation()) > FMath::Square(LoseTargetRange))
		AcquireCombatTarget();
	if (!CombatTarget) { bDirectChase = false; StopMovement(); bCanLaunchSpell = false; ClearFocus(EAIFocusPriority::Gameplay); return false; }
	SetFocus(CombatTarget);
	const float DistanceSquared = FVector::DistSquared(Self->GetActorLocation(), CombatTarget->GetActorLocation());
	TArray<URPGCombatAbilityDefinition*> Abilities;
	for (URPGCombatAbilityDefinition* Ability : CombatAbilities) if (Ability && Ability->IsConfigured()) Abilities.Add(Ability);
	if (Abilities.IsEmpty() && AttackAbility && AttackAbility->IsConfigured()) Abilities.Add(AttackAbility);
	float ApproachRange = TNumericLimits<float>::Max();
	for (URPGCombatAbilityDefinition* Ability : Abilities)
	{
		ApproachRange = FMath::Min(ApproachRange, Ability->Range);
		URPGAbilitySystemComponent* ASC = Cast<URPGAbilitySystemComponent>(Self->GetAbilitySystemComponent());
		if (DistanceSquared <= FMath::Square(Ability->Range) && LineOfSightTo(CombatTarget) && ASC &&
			ASC->ActivateAbilityDefinition(Ability, 1, CombatTarget))
		{
			bDirectChase = false; StopMovement(); bCanLaunchSpell = true; return true;
		}
	}
	if (!Abilities.IsEmpty() && DistanceSquared > FMath::Square(ApproachRange))
	{
		bCanLaunchSpell = false;
		const auto Result = MoveToActor(CombatTarget, FMath::Max(25.f, ApproachRange * 0.5f), false, true, false);
		// Clear training arenas also work before a navmesh build. Obstacles still require navigation.
		bDirectChase = Result == EPathFollowingRequestResult::Failed && LineOfSightTo(CombatTarget);
		return false;
	}
	bDirectChase = false; StopMovement(); bCanLaunchSpell = false;
	return false;
}
