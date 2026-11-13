#pragma once

#include "Animation/AnimInstance.h"
#include "RPGAnimInstance.generated.h"

class ARPGCharacter;
class UCharacterMovementComponent;

/**
 * Compatibility parent for animation Blueprints authored against the original
 * DuTraveller module. Gameplay remains implemented in the child Blueprints.
 * The cached owner/movement/state members below are the native surface the
 * legacy animation graphs read and write each frame.
 */
UCLASS(Blueprintable, BlueprintType)
class DUTRAVELLER_API URPGAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	UPROPERTY(BlueprintReadWrite, Category = "Character")
	TObjectPtr<ARPGCharacter> AICharacter;

	UPROPERTY(BlueprintReadWrite, Category = "Character")
	TObjectPtr<UCharacterMovementComponent> MoveComp;

	UPROPERTY(BlueprintReadWrite, Category = "Character")
	bool bCanLaunchSpell = false;

	UPROPERTY(BlueprintReadWrite, Category = "Character")
	bool bIsStunned = false;

	UPROPERTY(BlueprintReadWrite, Category = "Character")
	bool bInShockLoop = false;
};
