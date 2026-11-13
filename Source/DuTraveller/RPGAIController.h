#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "RPGAIController.generated.h"

class ARPGCharacter;
class URPGCombatAbilityDefinition;

/** Legacy AI controller base; parent of the missing BP_RPGAIController Blueprint class. */
UCLASS(BlueprintType, Blueprintable)
class DUTRAVELLER_API ARPGAIController : public AAIController
{
	GENERATED_BODY()

public:
	ARPGAIController();
	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(BlueprintReadWrite, Category = "AI")
	bool bCanLaunchSpell = false;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Combat") TObjectPtr<URPGCombatAbilityDefinition> AttackAbility;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Combat") TArray<TObjectPtr<URPGCombatAbilityDefinition>> CombatAbilities;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Combat") bool bTargetPlayersOnly = false;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Combat", meta=(ClampMin="100")) float DetectionRange = 1500.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Combat", meta=(ClampMin="100")) float LoseTargetRange = 2200.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Combat", meta=(ClampMin="0.05")) float DecisionInterval = 0.25f;
	UPROPERTY(BlueprintReadOnly, Category="AI|Combat") TObjectPtr<ARPGCharacter> CombatTarget;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="AI|Combat") bool RunCombatDecision();
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="AI|Combat") ARPGCharacter* AcquireCombatTarget();
private:
	float DecisionCountdown = 0.f;
	bool bDirectChase = false;
};
