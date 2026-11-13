#include "RPGPartyWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "EngineUtils.h"
#include "RPGPartyComponent.h"
#include "RPGPlayerState.h"

namespace
{
UButton* AddButton(UWidgetTree* Tree, UVerticalBox* Box, const FString& Label)
{
	UButton* Button = Tree->ConstructWidget<UButton>();
	UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(); Text->SetText(FText::FromString(Label));
	Button->SetContent(Text); Box->AddChildToVerticalBox(Button); return Button;
}
}

void URPGPartyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!WidgetTree->RootWidget)
	{
		UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(); WidgetTree->RootWidget = Root;
		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(); Title->SetText(NSLOCTEXT("DuTravellerParty", "Title", "Party")); Root->AddChildToVerticalBox(Title);
		StatusText = WidgetTree->ConstructWidget<UTextBlock>(); Root->AddChildToVerticalBox(StatusText);
		RosterText = WidgetTree->ConstructWidget<UTextBlock>(); Root->AddChildToVerticalBox(RosterText);
		UTextBlock* SizeLabel = WidgetTree->ConstructWidget<UTextBlock>(); SizeLabel->SetText(NSLOCTEXT("DuTravellerParty", "SizeLabel", "Party size (leader):")); Root->AddChildToVerticalBox(SizeLabel);
		PartySizeSelector = WidgetTree->ConstructWidget<UComboBoxString>();
		PartySizeSelector->AddOption(TEXT("4")); PartySizeSelector->AddOption(TEXT("10")); PartySizeSelector->AddOption(TEXT("20"));
		Root->AddChildToVerticalBox(PartySizeSelector);
		UButton* SetSize = AddButton(WidgetTree, Root, TEXT("Set Party Size")); SetSize->OnClicked.AddDynamic(this, &ThisClass::SetPartySizeClicked);
		InviteSelector = WidgetTree->ConstructWidget<UComboBoxString>(); Root->AddChildToVerticalBox(InviteSelector);
		UButton* Invite = AddButton(WidgetTree, Root, TEXT("Invite Selected Player")); Invite->OnClicked.AddDynamic(this, &ThisClass::InviteClicked);
		UButton* Accept = AddButton(WidgetTree, Root, TEXT("Accept Invite")); Accept->OnClicked.AddDynamic(this, &ThisClass::AcceptClicked);
		UButton* Decline = AddButton(WidgetTree, Root, TEXT("Decline Invite")); Decline->OnClicked.AddDynamic(this, &ThisClass::DeclineClicked);
		UButton* Leave = AddButton(WidgetTree, Root, TEXT("Leave Party")); Leave->OnClicked.AddDynamic(this, &ThisClass::LeaveClicked);
	}
	RefreshParty();
}

void URPGPartyWidget::InitializeParty(ARPGPlayerState* PlayerState)
{
	if (Party) { Party->OnPartyChanged.RemoveAll(this); Party->OnInviteChanged.RemoveAll(this); }
	Party = PlayerState ? PlayerState->Party : nullptr;
	if (Party) { Party->OnPartyChanged.AddDynamic(this, &ThisClass::PartyChanged); Party->OnInviteChanged.AddDynamic(this, &ThisClass::PartyChanged); }
	RefreshParty();
}

void URPGPartyWidget::RefreshParty()
{
	if (!StatusText || !RosterText || !InviteSelector) return;
	StatusText->SetText(FText::FromString(!Party ? TEXT("Party unavailable") :
		(Party->PendingInviter ? FString::Printf(TEXT("Invite from %s"), *Party->PendingInviter->GetPlayerName()) :
		(Party->IsInParty() ? FString::Printf(TEXT("Party members: %d / %d"), Party->Members.Num(), Party->PartySizeLimit) : FString::Printf(TEXT("Not in a party (size %d)"), Party->PartySizeLimit)))));
	if (PartySizeSelector && Party) PartySizeSelector->SetSelectedOption(FString::FromInt(Party->PartySizeLimit));
	FString Roster;
	if (Party) for (ARPGPlayerState* Member : Party->Members)
		if (IsValid(Member)) Roster += FString::Printf(TEXT("%s%s\n"), Member == Party->Leader ? TEXT("Leader: ") : TEXT("Member: "), *Member->GetPlayerName());
	RosterText->SetText(FText::FromString(Roster));
	InviteCandidates.Reset(); InviteSelector->ClearOptions();
	ARPGPlayerState* Self = Party ? Cast<ARPGPlayerState>(Party->GetOwner()) : nullptr;
	if (Self && GetWorld()) for (TActorIterator<ARPGPlayerState> It(GetWorld()); It; ++It)
		if (*It != Self && It->Party && !It->Party->IsInParty()) { InviteCandidates.Add(*It); InviteSelector->AddOption(It->GetPlayerName()); }
}

void URPGPartyWidget::InvitePlayer(ARPGPlayerState* PlayerState) { if (Party) Party->RequestInvite(PlayerState); }
void URPGPartyWidget::InviteClicked() { const int32 Index = InviteSelector ? InviteSelector->FindOptionIndex(InviteSelector->GetSelectedOption()) : INDEX_NONE; if (InviteCandidates.IsValidIndex(Index)) InvitePlayer(InviteCandidates[Index]); }
void URPGPartyWidget::SetPartySizeClicked() { if (Party && PartySizeSelector) Party->RequestSetPartySizeLimit(FCString::Atoi(*PartySizeSelector->GetSelectedOption())); }
void URPGPartyWidget::AcceptClicked() { if (Party) Party->AcceptInvite(); }
void URPGPartyWidget::DeclineClicked() { if (Party) Party->DeclineInvite(); }
void URPGPartyWidget::LeaveClicked() { if (Party) Party->LeaveParty(); }
void URPGPartyWidget::PartyChanged() { RefreshParty(); }
