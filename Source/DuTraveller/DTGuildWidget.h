#pragma once
#include "Blueprint/UserWidget.h"
#include "DTGuildWidget.generated.h"

class UTextBlock;
class ARPGGuildState;

/** Guild overview consumes the same replicated PlayerState as the other menus. */
UCLASS(BlueprintType, Blueprintable)
class DUTRAVELLER_API UDTGuildWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="DT|Guild") void RefreshGuild();
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
private:
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Summary;
	UPROPERTY(Transient) TObjectPtr<ARPGGuildState> BoundGuild;
	float RefreshElapsed = 0.f;
};
