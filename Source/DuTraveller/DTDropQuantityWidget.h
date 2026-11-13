#pragma once
#include "RPG_UserWidget.h"
#include "DTDropQuantityWidget.generated.h"

class UEditableTextBox;
class UVerticalBox;

/** Restores explicit quantity controls around the retained drop-confirmation graph. */
UCLASS(Blueprintable)
class DUTRAVELLER_API UDTDropQuantityWidget : public URPG_UserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DT|Drop Quantity") bool bBuildDefaultQuantityControls = true;
	UFUNCTION(BlueprintCallable, Category="DT|Drop Quantity") void IncreaseQuantity();
	UFUNCTION(BlueprintCallable, Category="DT|Drop Quantity") void DecreaseQuantity();
	UFUNCTION(BlueprintCallable, Category="DT|Drop Quantity") void ConfirmQuantity();
	UFUNCTION(BlueprintCallable, Category="DT|Drop Quantity") void CancelQuantity();
protected:
	virtual void NativeConstruct() override;
private:
	void AdjustQuantity(int32 Delta);
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> QuantityInput;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> QuantityPanel;
};
