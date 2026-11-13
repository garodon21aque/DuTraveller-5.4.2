#include "RPGPlayerState.h"
#include "RPGAbilitySystemComponent.h"
#include "RPGAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "RPGDynamicQuestComponent.h"
#include "RPGSkillComponent.h"
#include "RPGPartyComponent.h"

ARPGPlayerState::ARPGPlayerState()
{
	bReplicateUsingRegisteredSubObjectList = true;
	AbilitySystem = CreateDefaultSubobject<URPGAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	Attributes = CreateDefaultSubobject<URPGAttributeSet>(TEXT("Attributes"));
	DynamicQuests = CreateDefaultSubobject<URPGDynamicQuestComponent>(TEXT("DynamicQuests"));
	Skills = CreateDefaultSubobject<URPGSkillComponent>(TEXT("Skills"));
	Party = CreateDefaultSubobject<URPGPartyComponent>(TEXT("Party"));
	SetNetUpdateFrequency(30.f);
}

UAbilitySystemComponent* ARPGPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void ARPGPlayerState::BeginPlay()
{
	Super::BeginPlay();
	LastBroadcastLevel=LVL; // Loading/initial replication is not a level gain.
	if (HasAuthority() && bGrantStartingProgression && !bStartingProgressionGranted)
	{
		bStartingProgressionGranted = true;
		GrantProgressionPoints(FMath::Max(0, StartingAttributePoints), FMath::Max(0, StartingSkillPoints));
	}
}

void ARPGPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARPGPlayerState, LVL);
	DOREPLIFETIME_CONDITION(ARPGPlayerState, EXP, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ARPGPlayerState, MaxExperience, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ARPGPlayerState, RequiredXp, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ARPGPlayerState, AttributePoints, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ARPGPlayerState, SpellPoints, COND_OwnerOnly);
	DOREPLIFETIME(ARPGPlayerState, GuildState);
}

bool ARPGPlayerState::GrantProgressionPoints(const int32 NewAttributePoints, const int32 NewSpellPoints)
{
	if (!HasAuthority() || NewAttributePoints < 0 || NewSpellPoints < 0 ||
		AttributePoints > MAX_int32 - NewAttributePoints || SpellPoints > MAX_int32 - NewSpellPoints) return false;
	AttributePoints += NewAttributePoints;
	SpellPoints += NewSpellPoints;
	OnRep_Progression();
	ForceNetUpdate();
	return true;
}

bool ARPGPlayerState::GrantExperience(const float Amount, const bool bShareWithParty)
{
	if (!HasAuthority() || !FMath::IsFinite(Amount) || Amount < 0.f) return false;
	if (bShareWithParty && Party && Party->IsInParty())
	{
		bool bGranted = false;
		for (ARPGPlayerState* Member : Party->Members)
			if (IsValid(Member)) bGranted |= Member->GrantExperience(Amount, false);
		return bGranted;
	}
	if (!FMath::IsFinite(EXP) || !FMath::IsFinite(EXP + Amount)) return false;
	if (Amount == 0.f) return true;
	if (LVL < 1 || !FMath::IsFinite(RequiredXp) || RequiredXp <= 0.f || EXP < 0.f) return false;
	// RequiredXp is the authored cost of one level. Resolve all crossed
	// thresholds in one operation and preserve overflow for the next level.
	const double TotalExperience=static_cast<double>(EXP)+Amount;
	const double LevelsAvailable=FMath::FloorToDouble(TotalExperience/static_cast<double>(RequiredXp));
	const int32 LevelsGained=static_cast<int32>(FMath::Min(LevelsAvailable,static_cast<double>(MAX_int32-LVL)));
	if (LastBroadcastLevel==INDEX_NONE) LastBroadcastLevel=LVL;
	LVL+=LevelsGained;
	EXP=static_cast<float>(TotalExperience-static_cast<double>(LevelsGained)*RequiredXp);
	MaxExperience=RequiredXp; // Retained Blueprint compatibility: same threshold as the HUD.
	AttributePoints=static_cast<int32>(FMath::Min(static_cast<int64>(MAX_int32),static_cast<int64>(AttributePoints)+static_cast<int64>(LevelsGained)*FMath::Max(0,AttributePointsPerLevel)));
	SpellPoints=static_cast<int32>(FMath::Min(static_cast<int64>(MAX_int32),static_cast<int64>(SpellPoints)+static_cast<int64>(LevelsGained)*FMath::Max(0,SkillPointsPerLevel)));
	OnRep_Progression(); ForceNetUpdate();
	return true;
}

void ARPGPlayerState::RequestUpgradeAttribute(const FGameplayTag AttributeTag)
{
	if (HasAuthority()) TryUpgradeAttribute(AttributeTag);
	else ServerUpgradeAttribute(AttributeTag);
}

void ARPGPlayerState::ServerUpgradeAttribute_Implementation(const FGameplayTag AttributeTag)
{
	TryUpgradeAttribute(AttributeTag);
}

bool ARPGPlayerState::TryUpgradeAttribute(const FGameplayTag AttributeTag)
{
	if (!HasAuthority() || !AbilitySystem || AttributePointCost <= 0 ||
		AttributePoints < AttributePointCost || !FMath::IsFinite(AttributeIncreasePerPurchase) || AttributeIncreasePerPurchase <= 0.f) return false;
	const TMap<FGameplayTag, FGameplayAttribute> Primary = URPGAttributeSet::PrimaryAttributes();
	const FGameplayAttribute* Attribute = Primary.Find(AttributeTag);
	if (!Attribute) return false;
	const float NewBase = AbilitySystem->GetNumericAttributeBase(*Attribute) + AttributeIncreasePerPurchase;
	if (!FMath::IsFinite(NewBase)) return false;
	AttributePoints -= AttributePointCost;
	AbilitySystem->SetNumericAttributeBase(*Attribute, NewBase);
	OnRep_Progression();
	ForceNetUpdate();
	return true;
}

float ARPGPlayerState::GetAttributeValue(const FName AttributeName, const bool bBaseValue) const
{
	const FStructProperty* Property = FindFProperty<FStructProperty>(URPGAttributeSet::StaticClass(), AttributeName);
	if (!AbilitySystem || !Property || Property->Struct != FGameplayAttributeData::StaticStruct()) return 0.f;
	const FGameplayAttribute Attribute(const_cast<FStructProperty*>(Property));
	return bBaseValue ? AbilitySystem->GetNumericAttributeBase(Attribute) : AbilitySystem->GetNumericAttribute(Attribute);
}

void ARPGPlayerState::OnRep_Progression()
{
	const int32 PreviousLevel=LastBroadcastLevel;
	LastBroadcastLevel=LVL;
	if (PreviousLevel!=INDEX_NONE && LVL>PreviousLevel)
	{
		for (int64 Level=static_cast<int64>(PreviousLevel)+1; Level<=LVL; ++Level)
			OnPlayerLevelChangedDelegate.Broadcast(static_cast<int32>(Level));
	}
	OnXPPercentChangedDelegate.Broadcast(GetExperiencePercent());
	OnProgressionChanged.Broadcast();
}

float ARPGPlayerState::GetExperiencePercent() const
{
	return FMath::IsFinite(EXP) && FMath::IsFinite(RequiredXp) && RequiredXp>0.f ? FMath::Clamp(EXP/RequiredXp,0.f,1.f) : 0.f;
}

void ARPGPlayerState::CopyProperties(APlayerState* NewPlayerState)
{
	Super::CopyProperties(NewPlayerState);
	ARPGPlayerState* Target = Cast<ARPGPlayerState>(NewPlayerState);
	if (!Target || !HasAuthority()) return;
	Target->LVL = LVL; Target->EXP = EXP;
	Target->LastBroadcastLevel=LVL;
	Target->MaxExperience = MaxExperience; Target->RequiredXp = RequiredXp;
	Target->AttributePoints = AttributePoints; Target->SpellPoints = SpellPoints;
	Target->bStartingProgressionGranted = bStartingProgressionGranted;
	Target->DynamicQuests->RestoreQuestState(DynamicQuests->CaptureQuestState());
	Skills->CopyProgressTo(Target->Skills);
	for (TFieldIterator<FStructProperty> It(URPGAttributeSet::StaticClass()); It; ++It)
	{
		if (It->Struct != FGameplayAttributeData::StaticStruct()) continue;
		const FGameplayAttribute Attribute(*It);
		Target->AbilitySystem->SetNumericAttributeBase(Attribute, AbilitySystem->GetNumericAttributeBase(Attribute));
	}
}
