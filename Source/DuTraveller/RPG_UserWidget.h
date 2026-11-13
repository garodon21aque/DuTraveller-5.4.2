#pragma once

#include "Blueprint/UserWidget.h"
#include "RPG_UserWidget.generated.h"

/** Legacy native base retained so existing widget Blueprints keep their parent. */
UCLASS(Blueprintable, BlueprintType)
class DUTRAVELLER_API URPG_UserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	/** Resolves owner context before legacy Blueprint initialization/Construct executes. */
	UFUNCTION(BlueprintCallable, Category="DT|UI") void InitializePlayerContext();
	/** Assigns the controller object consumed by legacy Blueprint widgets. */
	UFUNCTION(BlueprintCallable, Category = "Widget Controller")
	void SetWidgetController(UObject* InWidgetController);

	UPROPERTY(BlueprintReadOnly, Category = "Widget Controller")
	TObjectPtr<UObject> WidgetController;

	/** Legacy spawn contract: Create Widget nodes pass the actor a widget is attached to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = "true"), Category = "World Widget")
	TObjectPtr<AActor> AttachedActor;

protected:
	/** Blueprint hook used by existing widgets to bind controller delegates. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Widget Controller")
	void WidgetControllerSet();
};
