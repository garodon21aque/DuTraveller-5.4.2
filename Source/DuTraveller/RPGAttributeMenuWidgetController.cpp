#include "RPGAttributeMenuWidgetController.h"
#include "RPGAttributeSet.h"
#include "RPGPlayerState.h"
#include "AbilitySystemComponent.h"

namespace
{
TMap<FGameplayTag, FGameplayAttribute> MenuAttributes()
{
	auto Attributes = URPGAttributeSet::PrimaryAttributes();
	for (TFieldIterator<FStructProperty> It(URPGAttributeSet::StaticClass()); It; ++It)
	{
		if (It->Struct != FGameplayAttributeData::StaticStruct()) continue;
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*(FString(TEXT("Attributes.Secondary.")) + It->GetName())), false);
		if (Tag.IsValid()) Attributes.Add(Tag, FGameplayAttribute(*It));
	}
	return Attributes;
}
}

void URPGAttributeMenuWidgetController::UpgradeAttribute(const FGameplayTag& AttributeTag)
{
	if (ARPGPlayerState* PS = Cast<ARPGPlayerState>(PlayerState)) PS->RequestUpgradeAttribute(AttributeTag);
}

void URPGAttributeMenuWidgetController::BroadcastInitialValues_Implementation()
{
	BindCallbacks();
	RefreshValues();
}

void URPGAttributeMenuWidgetController::BindCallbacks()
{
	ARPGPlayerState* PS = Cast<ARPGPlayerState>(PlayerState);
	if (BoundASC.Get() != AbilitySystemComponent.Get())
	{
		if (UAbilitySystemComponent* Previous = BoundASC.Get())
			for (const auto& Pair : MenuAttributes()) Previous->GetGameplayAttributeValueChangeDelegate(Pair.Value).RemoveAll(this);
		BoundASC = AbilitySystemComponent;
		if (AbilitySystemComponent)
			for (const auto& Pair : MenuAttributes())
				AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Pair.Value).AddUObject(this, &URPGAttributeMenuWidgetController::AttributeChanged);
	}
	if (BoundPlayerState.Get() != PS)
	{
		if (ARPGPlayerState* Previous = BoundPlayerState.Get()) Previous->OnProgressionChanged.RemoveDynamic(this, &URPGAttributeMenuWidgetController::RefreshValues);
		BoundPlayerState = PS;
		if (PS) PS->OnProgressionChanged.AddUniqueDynamic(this, &URPGAttributeMenuWidgetController::RefreshValues);
	}
}

void URPGAttributeMenuWidgetController::AttributeChanged(const FOnAttributeChangeData& Data)
{
	RefreshValues();
}

void URPGAttributeMenuWidgetController::RefreshValues()
{
	if (AbilitySystemComponent)
	{
		for (const auto& Pair : MenuAttributes())
		{
			FRPG_SAttributeInfo Info;
			Info.AttributeTag = Pair.Key;
			Info.AttributeName = FText::FromString(Pair.Value.GetName());
			Info.AttributeValue = AbilitySystemComponent->GetNumericAttribute(Pair.Value);
			AttributeInfoDelegate.Broadcast(Info);
		}
	}
	if (const ARPGPlayerState* PS = Cast<ARPGPlayerState>(PlayerState)) AttributePointsChangedDelegate.Broadcast(PS->AttributePoints);
}
