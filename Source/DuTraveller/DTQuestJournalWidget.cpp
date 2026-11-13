#include "DTQuestJournalWidget.h"
#include "RPGPlayerState.h"
#include "RPGDynamicQuestComponent.h"
#include "RPGRandomQuestSpawner.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"

void UDTQuestJournalEntry::Configure(FText Title, FText Summary, FGuid Id, ARPGRandomQuestSpawner* OfferSource, bool bCanAct)
{
	QuestTitle = Title; QuestSummary = Summary; RequestId = Id; Spawner = OfferSource;
	bCanPerformAction = bCanAct;
	ActionLabel = FText::FromString(Spawner ? TEXT("Accept quest") : TEXT("Claim rewards"));
	QuestEntryChanged();
}

TSharedRef<SWidget> UDTQuestJournalEntry::RebuildWidget()
{
	// Blueprint row classes can supply their own tree and call Perform Quest Action.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
		WidgetTree->RootWidget = Box;
		for (const FText& Text : {QuestTitle, QuestSummary})
		{
			UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
			Label->SetText(Text); Label->SetAutoWrapText(true);
			Box->AddChildToVerticalBox(Label);
		}
		if (bCanPerformAction)
		{
			UButton* Action = WidgetTree->ConstructWidget<UButton>();
			UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
			Label->SetText(ActionLabel); Action->SetContent(Label);
			Action->OnClicked.AddUniqueDynamic(this, &ThisClass::PerformQuestAction);
			Box->AddChildToVerticalBox(Action);
		}
	}
	return Super::RebuildWidget();
}

void UDTQuestJournalEntry::PerformQuestAction()
{
	ARPGPlayerState* State = GetOwningPlayerState<ARPGPlayerState>();
	if (!bCanPerformAction || !State || !State->DynamicQuests) return;
	if (Spawner) State->DynamicQuests->RequestAcceptOffer(Spawner, RequestId);
	else State->DynamicQuests->RequestClaimReward(RequestId);
}

void UDTQuestJournalWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (bBuildDefaultJournal && WidgetTree && !JournalEntries)
		if (UPanelWidget* Host = Cast<UPanelWidget>(GetWidgetFromName(JournalHostName)))
		{
			JournalEntries = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DT_DynamicQuestEntries"));
			Host->AddChild(JournalEntries);
		}
	LastSnapshot.Reset();
	RefreshJournal();
}

void UDTQuestJournalWidget::NativeDestruct()
{
	if (BoundQuests) BoundQuests->OnQuestsChanged.RemoveAll(this);
	BoundQuests = nullptr;
	Super::NativeDestruct();
}

void UDTQuestJournalWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	RefreshElapsed += DeltaTime;
	if (RefreshElapsed >= 0.5f) { RefreshElapsed = 0.f; RefreshJournal(); }
}

void UDTQuestJournalWidget::RefreshJournal()
{
	ARPGPlayerState* State = GetOwningPlayerState<ARPGPlayerState>();
	URPGDynamicQuestComponent* Quests = State ? State->DynamicQuests.Get() : nullptr;
	if (BoundQuests != Quests)
	{
		if (BoundQuests) BoundQuests->OnQuestsChanged.RemoveAll(this);
		BoundQuests = Quests;
		if (Quests) Quests->OnQuestsChanged.AddUniqueDynamic(this, &ThisClass::RefreshJournal);
	}
	if (!Quests) return;
	FString Snapshot;
	for (const FRPGDynamicQuestRecord& Record : Quests->Records)
	{
		Snapshot += Record.InstanceId.ToString() + FString::FromInt(static_cast<int32>(Record.Status));
		for (int32 Progress : Record.Progress) Snapshot += TEXT(":") + FString::FromInt(Progress);
	}
	TArray<TPair<ARPGRandomQuestSpawner*, FRPGQuestOffer>> Nearby;
	if (APawn* Pawn = GetOwningPlayerPawn())
		for (TActorIterator<ARPGRandomQuestSpawner> It(GetWorld()); It; ++It)
			if (It->ContainsPosition(Pawn->GetActorLocation()))
				for (const FRPGQuestOffer& Offer : It->Offers)
				{
					if (!Offer.Definition || Offer.ExpiresAt <= GetWorld()->GetTimeSeconds()) continue;
					const bool bAlreadyOwned = Quests->Records.ContainsByPredicate([&](const FRPGDynamicQuestRecord& Record) { return Record.SourceOfferId == Offer.OfferId; });
					if (bAlreadyOwned) continue;
					Snapshot += Offer.OfferId.ToString();
					Nearby.Emplace(*It, Offer);
				}
	Snapshot += TEXT("#") + FString::FromInt(Quests->Records.Num()) + TEXT(":") + FString::FromInt(Nearby.Num());
	if (Snapshot == LastSnapshot) return;
	LastSnapshot = Snapshot;
	OwnedQuestCount = Quests->Records.Num(); NearbyOfferCount = Nearby.Num();
	if (JournalEntries)
	{
		JournalEntries->ClearChildren();
		UClass* RowClass = EntryWidgetClass ? EntryWidgetClass.Get() : UDTQuestJournalEntry::StaticClass();
		for (const FRPGDynamicQuestRecord& Record : Quests->Records)
		{
			if (!Record.Definition) continue;
			FString Summary = Record.Definition->Description.ToString();
			for (int32 Index = 0; Index < Record.Definition->Objectives.Num(); ++Index)
			{
				const FRPGDynamicObjective& Objective = Record.Definition->Objectives[Index];
				Summary += FString::Printf(TEXT("\n%s: %d / %d"), *Objective.ObjectiveId.ToString(), Record.Progress.IsValidIndex(Index) ? Record.Progress[Index] : 0, Objective.RequiredCount);
			}
			Summary += Record.Status == ERPGDynamicQuestStatus::Claimed ? TEXT("\nRewards claimed") : Record.Status == ERPGDynamicQuestStatus::Complete ? TEXT("\nComplete") : TEXT("\nIn progress");
			UDTQuestJournalEntry* Row = CreateWidget<UDTQuestJournalEntry>(GetOwningPlayer(), RowClass);
			Row->Configure(Record.Definition->Title, FText::FromString(Summary), Record.InstanceId, nullptr, Record.Status == ERPGDynamicQuestStatus::Complete);
			JournalEntries->AddChildToVerticalBox(Row);
		}
		for (const auto& Pair : Nearby)
		{
			UDTQuestJournalEntry* Row = CreateWidget<UDTQuestJournalEntry>(GetOwningPlayer(), RowClass);
			Row->Configure(Pair.Value.Definition->Title, Pair.Value.Definition->Description, Pair.Value.OfferId, Pair.Key, true);
			JournalEntries->AddChildToVerticalBox(Row);
		}
		if (OwnedQuestCount == 0 && NearbyOfferCount == 0)
		{
			UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>();
			Empty->SetText(FText::FromString(TEXT("No active quests or nearby quest offers.")));
			Empty->SetAutoWrapText(true); JournalEntries->AddChildToVerticalBox(Empty);
		}
	}
	QuestJournalChanged();
}
