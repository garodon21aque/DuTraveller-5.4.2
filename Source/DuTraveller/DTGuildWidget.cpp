#include "DTGuildWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "RPGGuildState.h"
#include "ItemObject.h"
#include "RPGPlayerState.h"

TSharedRef<SWidget> UDTGuildWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
		Background->SetBrushColor(FLinearColor(0.025f, 0.035f, 0.05f, 0.96f));
		Background->SetPadding(FMargin(40.f));
		Summary = WidgetTree->ConstructWidget<UTextBlock>();
		Summary->SetAutoWrapText(true);
		Background->SetContent(Summary);
		WidgetTree->RootWidget = Background;
	}
	return Super::RebuildWidget();
}
void UDTGuildWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshGuild();
}
void UDTGuildWidget::NativeDestruct()
{
	if (BoundGuild) BoundGuild->OnGuildChanged.RemoveAll(this);
	BoundGuild = nullptr;
	Super::NativeDestruct();
}
void UDTGuildWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	RefreshElapsed += DeltaTime;
	if (RefreshElapsed >= 0.5f) { RefreshElapsed = 0.f; RefreshGuild(); }
}
void UDTGuildWidget::RefreshGuild()
{
	const ARPGPlayerState* State = GetOwningPlayerState<ARPGPlayerState>();
	ARPGGuildState* Guild = State ? State->GuildState.Get() : nullptr;
	if (BoundGuild != Guild)
	{
		if (BoundGuild) BoundGuild->OnGuildChanged.RemoveAll(this);
		BoundGuild = Guild;
		if (BoundGuild) BoundGuild->OnGuildChanged.AddUniqueDynamic(this, &ThisClass::RefreshGuild);
	}
	if (!Summary) return;
	if (!Guild) { Summary->SetText(NSLOCTEXT("DTUI", "NoGuild", "Guild\n\nYou are not currently a member of a guild.")); return; }
	FString Text = FString::Printf(TEXT("Guild: %s\nReputation: %d\n\nMembers\n"), *Guild->GuildName.ToString(), Guild->Reputation);
	for (const FRPGGuildMember& Member : Guild->Members)
		if (Member.Player) Text += FString::Printf(TEXT("%s — %s\n"), *Member.Player->GetPlayerName(), *UEnum::GetValueAsString(Member.Role));
	Text += TEXT("\nShared bank\n");
	for (const FRPGGuildBankStack& Stack : Guild->Bank)
		Text += FString::Printf(TEXT("%s × %d\n"), *GetNameSafe(Stack.ItemClass.Get()), Stack.Quantity);
	Text += FString::Printf(TEXT("\nSettlement buildings: %d"), Guild->Buildings.Num());
	Summary->SetText(FText::FromString(Text));
}
