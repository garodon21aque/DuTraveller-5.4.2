#pragma once

#include "RPGAbilityInfo.h"
#include "RPGWidgetController.h"
#include "RPGOverlayWidgetController.generated.h"

struct FOnAttributeChangeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAttributeChangedSignature, float, NewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerLevelChangedSignature, int32, NewValue, bool, bLevelUp);

/** Native contract for the HUD/overlay GAS presentation layer. */
UCLASS(BlueprintType, Blueprintable)
class DUTRAVELLER_API URPGOverlayWidgetController : public URPGWidgetController
{
	GENERATED_BODY()

public:
	virtual void BroadcastInitialValues_Implementation() override;
	void BindCallbacks();
	UPROPERTY(BlueprintAssignable, Category = "GAS|Attribute")
	FOnAttributeChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Attribute")
	FOnAttributeChangedSignature OnMaxHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Attribute")
	FOnAttributeChangedSignature OnManaChanged;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Attribute")
	FOnAttributeChangedSignature OnMaxManaChanged;

	UPROPERTY(BlueprintAssignable, Category = "GAS|XP")
	FOnAttributeChangedSignature OnXPPercentChangedDelegate;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Level")
	FOnPlayerLevelChangedSignature OnPlayerLevelChanged;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Level")
	FOnPlayerLevelChangedSignature OnPlayerLevelChangedDelegate;
private:
	int32 LastPresentedLevel = INDEX_NONE;
	float LastBroadcastXPPercent = -1.f;
	bool bBroadcastingInitialValues = false;
	void AttributeChanged(const FOnAttributeChangeData& Data);
	UFUNCTION() void RefreshProgression();
	UFUNCTION() void RefreshSkills();
	TWeakObjectPtr<class URPGSkillComponent> BoundSkills;
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	TWeakObjectPtr<class ARPGPlayerState> BoundPlayerState;
};
