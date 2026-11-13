#include "RPGAnimInstance.h"
#include "RPGCharacter.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/UnrealType.h"

void URPGAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	AICharacter = Cast<ARPGCharacter>(TryGetPawnOwner());
	MoveComp = AICharacter ? AICharacter->GetCharacterMovement() : nullptr;
	const UAbilitySystemComponent* ASC = AICharacter ? AICharacter->GetAbilitySystemComponent() : nullptr;
	const FGameplayTag Stunned = FGameplayTag::RequestGameplayTag(TEXT("State.Stunned"), false);
	bIsStunned = AICharacter && (AICharacter->bIsStunned ||
		(ASC && Stunned.IsValid() && ASC->HasMatchingGameplayTag(Stunned)));
	// The retained locomotion blendspace reads GroundSpeed but its old update
	// graph never assigned it. Supply movement data on the existing native parent.
	if (FNumericProperty* Speed = FindFProperty<FNumericProperty>(GetClass(), TEXT("GroundSpeed")))
		if (Speed->IsFloatingPoint()) Speed->SetFloatingPointPropertyValue(Speed->ContainerPtrToValuePtr<void>(this), AICharacter ? AICharacter->GetVelocity().Size2D() : 0.f);
	if (FBoolProperty* Falling = FindFProperty<FBoolProperty>(GetClass(), TEXT("IsFalling")))
		Falling->SetPropertyValue_InContainer(this, MoveComp && MoveComp->IsFalling());
}
