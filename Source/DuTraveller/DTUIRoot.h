#pragma once

#include "Blueprint/UserWidget.h"
#include "DTUIRoot.generated.h"

class UOverlay;
class UWidget;

/** One viewport root per local player. Panels remain separate reusable widgets. */
UCLASS(BlueprintType, Blueprintable)
class DUTRAVELLER_API UDTUIRoot : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetMenuInteractionActive(bool bActive);
	/** Compatibility entry point for retained HUD Create Widget graphs. */
	UFUNCTION(BlueprintCallable, Category="DT|UI", meta=(WorldContext="WorldContextObject", DefaultToSelf="WorldContextObject"))
	static void AttachPlayerNotification(const UObject* WorldContextObject);
	UFUNCTION(BlueprintCallable, Category="DT|UI") void AttachHUD(UUserWidget* Widget);
	UFUNCTION(BlueprintCallable, Category="DT|UI") void AttachPanel(UUserWidget* Widget);
	UFUNCTION(BlueprintCallable, Category="DT|UI") void AttachNotification(UUserWidget* Widget);
protected:
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	void EnsureLayers();
	UPROPERTY(Transient) TObjectPtr<UOverlay> HUDLayer;
	UPROPERTY(Transient) TObjectPtr<UOverlay> MenuLayer;
	UPROPERTY(Transient) TObjectPtr<UOverlay> NotificationLayer;
};
