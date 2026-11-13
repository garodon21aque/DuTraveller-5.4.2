#pragma once

#include "RPG_UserWidget.h"
#include "DTQuestJournalWidget.generated.h"

class UVerticalBox;
class UTextBlock;
class UButton;
class URPGDynamicQuestComponent;
class ARPGRandomQuestSpawner;

/** A Blueprint-replaceable row for an owned quest or a nearby authoritative offer. */
UCLASS(BlueprintType, Blueprintable)
class DUTRAVELLER_API UDTQuestJournalEntry : public URPG_UserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly, Category="DT|Quests") FText QuestTitle;
	UPROPERTY(BlueprintReadOnly, Category="DT|Quests") FText QuestSummary;
	UPROPERTY(BlueprintReadOnly, Category="DT|Quests") FText ActionLabel;
	UPROPERTY(BlueprintReadOnly, Category="DT|Quests") bool bCanPerformAction = false;
	void Configure(FText Title, FText Summary, FGuid Id, ARPGRandomQuestSpawner* OfferSource, bool bCanAct);
	UFUNCTION(BlueprintCallable, Category="DT|Quests") void PerformQuestAction();
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	UFUNCTION(BlueprintImplementableEvent, Category="DT|Quests") void QuestEntryChanged();
private:
	FGuid RequestId;
	UPROPERTY(Transient) TObjectPtr<ARPGRandomQuestSpawner> Spawner;
};

/** Bridges PlayerState quest progress into the retained quest log's designer layout. */
UCLASS(BlueprintType, Blueprintable)
class DUTRAVELLER_API UDTQuestJournalWidget : public URPG_UserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DT|Quests") bool bBuildDefaultJournal = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DT|Quests") FName JournalHostName = TEXT("MainSb");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DT|Quests") TSubclassOf<UDTQuestJournalEntry> EntryWidgetClass;
	UPROPERTY(BlueprintReadOnly, Category="DT|Quests") int32 OwnedQuestCount = 0;
	UPROPERTY(BlueprintReadOnly, Category="DT|Quests") int32 NearbyOfferCount = 0;
	UFUNCTION(BlueprintCallable, Category="DT|Quests") void RefreshJournal();
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	UFUNCTION(BlueprintImplementableEvent, Category="DT|Quests") void QuestJournalChanged();
private:
	UPROPERTY(Transient, BlueprintReadOnly, Category="DT|Quests", meta=(AllowPrivateAccess="true")) TObjectPtr<UVerticalBox> JournalEntries;
	UPROPERTY(Transient) TObjectPtr<URPGDynamicQuestComponent> BoundQuests;
	FString LastSnapshot;
	float RefreshElapsed = 0.f;
};
