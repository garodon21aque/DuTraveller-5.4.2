#pragma once

#include "Components/ActorComponent.h"
#include "RPGInteractionComponent.generated.h"

class ARPGAICharacter;

/** Legacy interaction component carried by actors that drive world widgets. */
UCLASS(ClassGroup = (Interaction), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class DUTRAVELLER_API URPGInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	TObjectPtr<UObject> ActionComp;

	// Serialized graphs read this as a variable get on the component
	// (e.g. WPB_TargetEnemy drives its health bar from the enemy actor).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	TObjectPtr<ARPGAICharacter> EnemyActor;

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetActionComponent(FName TargetType);
};
