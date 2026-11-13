#pragma once

#include "ItemObject.h"
#include "GameplayEffect.h"
#include "Consumable_ItemObject.generated.h"

/** Separate from equipment. Concrete food/potion definitions supply their real GAS effect. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class DUTRAVELLER_API UConsumable_ItemObject : public UItemObject
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Consumable")
	TSubclassOf<UGameplayEffect> UseEffect;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Consumable", meta=(ClampMin="0.001"))
	float EffectLevel = 1.f;
	/** Named magnitudes for the configured effect, e.g. Health=25 for resource restoration. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Consumable")
	TMap<FName, float> SetByCallerMagnitudes;
	virtual bool ApplyUseEffect_Implementation(AActor* User) override;
};
