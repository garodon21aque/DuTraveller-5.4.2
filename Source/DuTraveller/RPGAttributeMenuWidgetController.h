#pragma once

#include "RPGWidgetController.h"
#include "RPG_AttributeInfo.h"
#include "RPGAttributeMenuWidgetController.generated.h"

class ARPGPlayerState;
struct FOnAttributeChangeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAttributeInfoSignature, const FRPG_SAttributeInfo&, Info);

/** Native contract consumed by the legacy attribute-menu Blueprints. */
UCLASS(BlueprintType, Blueprintable)
class DUTRAVELLER_API URPGAttributeMenuWidgetController : public URPGWidgetController
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "GAS|Attribute")
	FAttributeInfoSignature AttributeInfoDelegate;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Attribute")
	FOnPlayerStatChangedSignature AttributePointsChangedDelegate;

	UFUNCTION(BlueprintCallable, Category = "GAS|Attribute")
	void UpgradeAttribute(const FGameplayTag& AttributeTag);
	virtual void BroadcastInitialValues_Implementation() override;
	void BindCallbacks();
	UFUNCTION() void RefreshValues();
private:
	void AttributeChanged(const FOnAttributeChangeData& Data);
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	TWeakObjectPtr<ARPGPlayerState> BoundPlayerState;
};
