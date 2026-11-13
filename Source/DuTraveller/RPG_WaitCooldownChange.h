#pragma once

#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "RPG_WaitCooldownChange.generated.h"

class UAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCooldownChangeSignature, float, TimeRemaining);

/** Blueprint async task used by spell globes to observe a cooldown tag. */
UCLASS(BlueprintType, meta = (ExposedAsyncProxy = "AsyncTask"))
class DUTRAVELLER_API URPG_WaitCooldownChange : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FCooldownChangeSignature CooldownStart;

	UPROPERTY(BlueprintAssignable)
	FCooldownChangeSignature CooldownEnd;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"))
	static URPG_WaitCooldownChange* WaitForCooldownChange(
		UAbilitySystemComponent* AbilitySystemComponent,
		FGameplayTag InCooldownTag);

	UFUNCTION(BlueprintCallable)
	void EndTask();

private:
	void CooldownTagChanged(const FGameplayTag ChangedTag, int32 NewCount);
	void OnActiveEffectAdded(
		UAbilitySystemComponent* TargetAbilitySystemComponent,
		const FGameplayEffectSpec& SpecApplied,
		FActiveGameplayEffectHandle ActiveEffectHandle);
	void BroadcastCurrentCooldown();

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> ObservedAbilitySystemComponent;

	FGameplayTag CooldownTag;
};
