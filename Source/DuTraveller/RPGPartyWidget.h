#pragma once

#include "Blueprint/UserWidget.h"
#include "RPGPartyWidget.generated.h"

class ARPGPlayerState;
class URPGPartyComponent;
class UTextBlock;
class UComboBoxString;
class UVerticalBox;

/** Owning-client party panel. Gameplay mutations remain server RPCs on RPGPartyComponent. */
UCLASS(BlueprintType, Blueprintable)
class DUTRAVELLER_API URPGPartyWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeConstruct() override;
	UFUNCTION(BlueprintCallable, Category="Party") void InitializeParty(ARPGPlayerState* PlayerState);
	UFUNCTION(BlueprintCallable, Category="Party") void InvitePlayer(ARPGPlayerState* PlayerState);
	UFUNCTION(BlueprintCallable, Category="Party") void RefreshParty();
	UPROPERTY(BlueprintReadOnly, Category="Party") TObjectPtr<URPGPartyComponent> Party;
private:
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RosterText;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> InviteSelector;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> PartySizeSelector;
	UPROPERTY(Transient) TArray<TObjectPtr<ARPGPlayerState>> InviteCandidates;
	UFUNCTION() void AcceptClicked();
	UFUNCTION() void DeclineClicked();
	UFUNCTION() void LeaveClicked();
	UFUNCTION() void InviteClicked();
	UFUNCTION() void SetPartySizeClicked();
	UFUNCTION() void PartyChanged();
};
