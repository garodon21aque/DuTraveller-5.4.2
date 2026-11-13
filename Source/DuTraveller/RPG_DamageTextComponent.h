#pragma once

#include "Components/WidgetComponent.h"
#include "RPG_DamageTextComponent.generated.h"

/** Legacy world-space damage text component base. */
UCLASS(Blueprintable, BlueprintType, ClassGroup=(UI), meta=(BlueprintSpawnableComponent))
class DUTRAVELLER_API URPG_DamageTextComponent : public UWidgetComponent
{
	GENERATED_BODY()
public:
	/** Pushes resolved damage into the WBP_DMGPopup widget, then self-destructs. */
	UFUNCTION(BlueprintCallable, Category = "Damage Text")
	void ShowDamageText(float Damage, bool bBlockedHit, bool bCriticalHit);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Text", meta = (ClampMin = "0.1"))
	float PopupLifetime = 1.0f;

private:
	FTimerHandle DestroyTimer;
};
