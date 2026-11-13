#pragma once

#include "Blueprint/UserWidget.h"

class URPGInteractionComponent;
#include "RPGWorldUserWidget.generated.h"



/** Legacy native base for widgets displayed in world space. */
UCLASS(Blueprintable, BlueprintType)
class DUTRAVELLER_API URPGWorldUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
	UFUNCTION(BlueprintCallable, Category="World Widget") void InitializeCharacterOverlay(class ARPGCharacter* Character, bool bFollowActor);
	UFUNCTION(BlueprintPure, Category="World Widget") bool IsStatusIconVisible(FName Status) const;
	UPROPERTY(Transient, BlueprintReadOnly, Category="World Widget") float DisplayedHealthPercent = 1.f;
	UPROPERTY(BlueprintReadOnly, Category = "Widget Controller")
	TObjectPtr<UObject> WidgetController;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = "true"), Category = "World Widget")
	TObjectPtr<AActor> AttachedActor;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = "true"), Category = "World Widget")
	TObjectPtr<URPGInteractionComponent> OwningInteractionComponent;



	UFUNCTION(BlueprintCallable, Category = "Widget Controller")
	void SetWidgetController(UObject* InWidgetController);

	UFUNCTION(BlueprintImplementableEvent, Category = "Widget Controller")
	void WidgetControllerSet();
private:
	bool bCharacterOverlay = false;
	bool bFollowCharacter = false;
	bool bStatusRowCreated = false;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<class UImage>> StatusIcons;
	void EnsureStatusRow();
};
