#include "RPGOverlayWidgetController.h"
#include "RPGPlayerState.h"
#include "RPGAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "RPGSkillComponent.h"

void URPGOverlayWidgetController::BroadcastInitialValues_Implementation()
{
	BindCallbacks();
	if (const URPGAttributeSet* Stats = Cast<URPGAttributeSet>(AttributeSet))
	{
		OnHealthChanged.Broadcast(Stats->GetHealth()); OnMaxHealthChanged.Broadcast(Stats->GetMaxHealth());
		OnManaChanged.Broadcast(Stats->GetMana()); OnMaxManaChanged.Broadcast(Stats->GetMaxMana());
	}
	bBroadcastingInitialValues=true;
	RefreshProgression();
	bBroadcastingInitialValues=false;
	RefreshSkills();
}

void URPGOverlayWidgetController::BindCallbacks()
{
	ARPGPlayerState* PS = Cast<ARPGPlayerState>(PlayerState);
	URPGSkillComponent* Skills = PS ? PS->Skills.Get() : nullptr;
	if (BoundSkills.Get() != Skills)
	{
		if (BoundSkills.IsValid()) BoundSkills->OnSkillsChanged.RemoveAll(this);
		BoundSkills = Skills;
		if (Skills) Skills->OnSkillsChanged.AddUniqueDynamic(this, &ThisClass::RefreshSkills);
	}
	if (BoundASC.Get() != AbilitySystemComponent.Get())
	{
		if (UAbilitySystemComponent* Previous = BoundASC.Get())
		{
			Previous->GetGameplayAttributeValueChangeDelegate(URPGAttributeSet::GetHealthAttribute()).RemoveAll(this);
			Previous->GetGameplayAttributeValueChangeDelegate(URPGAttributeSet::GetMaxHealthAttribute()).RemoveAll(this);
			Previous->GetGameplayAttributeValueChangeDelegate(URPGAttributeSet::GetManaAttribute()).RemoveAll(this);
			Previous->GetGameplayAttributeValueChangeDelegate(URPGAttributeSet::GetMaxManaAttribute()).RemoveAll(this);
		}
		BoundASC = AbilitySystemComponent;
		if (AbilitySystemComponent)
		{
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(URPGAttributeSet::GetHealthAttribute()).AddUObject(this, &URPGOverlayWidgetController::AttributeChanged);
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(URPGAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &URPGOverlayWidgetController::AttributeChanged);
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(URPGAttributeSet::GetManaAttribute()).AddUObject(this, &URPGOverlayWidgetController::AttributeChanged);
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(URPGAttributeSet::GetMaxManaAttribute()).AddUObject(this, &URPGOverlayWidgetController::AttributeChanged);
		}
	}
	if (BoundPlayerState.Get() != PS)
	{
		if (ARPGPlayerState* Previous = BoundPlayerState.Get()) Previous->OnProgressionChanged.RemoveDynamic(this, &URPGOverlayWidgetController::RefreshProgression);
		BoundPlayerState = PS;
		LastPresentedLevel=INDEX_NONE;
		LastBroadcastXPPercent=-1.f;
		if (PS) PS->OnProgressionChanged.AddUniqueDynamic(this, &URPGOverlayWidgetController::RefreshProgression);
	}
}

void URPGOverlayWidgetController::AttributeChanged(const FOnAttributeChangeData& Data)
{
	if (Data.Attribute == URPGAttributeSet::GetHealthAttribute()) OnHealthChanged.Broadcast(Data.NewValue);
	else if (Data.Attribute == URPGAttributeSet::GetMaxHealthAttribute()) OnMaxHealthChanged.Broadcast(Data.NewValue);
	else if (Data.Attribute == URPGAttributeSet::GetManaAttribute()) OnManaChanged.Broadcast(Data.NewValue);
	else if (Data.Attribute == URPGAttributeSet::GetMaxManaAttribute()) OnMaxManaChanged.Broadcast(Data.NewValue);
}

void URPGOverlayWidgetController::RefreshSkills()
{
	if (BoundSkills.IsValid()) for (const FRPGAbilityInfo& Info : BoundSkills->GetAbilityPresentation()) AbilityInfoDelegate.Broadcast(Info);
}

void URPGOverlayWidgetController::RefreshProgression()
{
	if (const ARPGPlayerState* PS = Cast<ARPGPlayerState>(PlayerState))
	{
		// XP notifications must stay distinct from level/stat refreshes.
		const float Percent=PS->GetExperiencePercent();
		if (bBroadcastingInitialValues || Percent!=LastBroadcastXPPercent)
		{
			LastBroadcastXPPercent=Percent;
			OnXPPercentChangedDelegate.Broadcast(Percent);
		}
		if (bBroadcastingInitialValues || LastPresentedLevel!=PS->LVL)
		{
			const bool bLevelUp=!bBroadcastingInitialValues && LastPresentedLevel!=INDEX_NONE && PS->LVL>LastPresentedLevel;
			const int32 PreviousLevel=LastPresentedLevel;
			LastPresentedLevel=PS->LVL;
			if (bLevelUp)
			{
				for (int64 Level=static_cast<int64>(PreviousLevel)+1; Level<=PS->LVL; ++Level)
				{
					OnPlayerLevelChanged.Broadcast(static_cast<int32>(Level),true);
					OnPlayerLevelChangedDelegate.Broadcast(static_cast<int32>(Level),true);
				}
			}
			else
			{
				OnPlayerLevelChanged.Broadcast(PS->LVL,false);
				OnPlayerLevelChangedDelegate.Broadcast(PS->LVL,false);
			}
		}
	}
}
