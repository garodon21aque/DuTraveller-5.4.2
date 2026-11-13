#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "../RPGCharacter.h"
#include "../RPGPlayerCharacter.h"
#include "../RPGPlayerState.h"
#include "../RPGAttributeSet.h"
#include "../AttributeComponent.h"
#include "../InventoryComponent.h"
#include "../Gear_ItemObject.h"
#include "../RPGCombatTypes.h"
#include "../RPGActionComponent.h"
#include "../RPGAbilitySystemComponent.h"
#include "../RPGGameplayAbility.h"
#include "../RPGSkillComponent.h"
#include "../RPGSkillExecution.h"
#include "../DTAnimationRouter.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Animation/AnimMontage.h"

namespace
{
struct FGASWorld
{
	UWorld* World;
	explicit FGASWorld(bool bPhysics = false)
	{
		const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
			.CreatePhysicsScene(bPhysics).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
	}
	~FGASWorld()
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
	ARPGCharacter* Character()
	{
		ARPGCharacter* Result = World->SpawnActor<ARPGCharacter>();
		Result->Initialize(nullptr);
		return Result;
	}
	void TickSeconds(const float Seconds, const float Step = .05f)
	{
		float Elapsed = 0.f;
		while (Elapsed < Seconds)
		{
			const float Delta = FMath::Min(Step, Seconds - Elapsed);
			World->Tick(LEVELTICK_All, Delta);
			Elapsed += Delta;
		}
	}
};

ARPGCharacter* BoundCharacter(FGASWorld& Context, ARPGPlayerState*& OutState)
{
	OutState = Context.World->SpawnActor<ARPGPlayerState>();
	ARPGCharacter* Pawn = Context.Character();
	Pawn->SetPlayerState(OutState);
	Pawn->Initialize(nullptr);
	return Pawn;
}

void SetBase(ARPGCharacter* Character, const FGameplayAttribute& Attribute, const float Value)
{
	Character->GetAbilitySystemComponent()->SetNumericAttributeBase(Attribute, Value);
}

void ResetVitals(ARPGCharacter* Character, const float Health = 1000.f)
{
	SetBase(Character, URPGAttributeSet::GetMaxHealthAttribute(), Health);
	SetBase(Character, URPGAttributeSet::GetHealthAttribute(), Health);
}

URPGCombatAbilityDefinition* MakeAbility(UObject* Outer, const FName Id, const ERPGAbilityKind Kind,
	const float BaseDamage, const float Range = 500.f)
{
	URPGCombatAbilityDefinition* Ability = NewObject<URPGCombatAbilityDefinition>(Outer);
	Ability->AbilityId = Id;
	Ability->Kind = Kind;
	Ability->BaseDamageByRank = {BaseDamage};
	Ability->AttackPowerScale = 1.f;
	Ability->Range = Range;
	Ability->Cooldown = 0.f;
	return Ability;
}

int32 CountEffectsOfClass(UAbilitySystemComponent* ASC, const UClass* EffectClass)
{
	int32 Count = 0;
	if (!ASC) return 0;
	for (const FActiveGameplayEffectHandle& Handle : ASC->GetActiveEffects(FGameplayEffectQuery()))
		if (const FActiveGameplayEffect* Effect = ASC->GetActiveGameplayEffect(Handle))
			if (Effect->Spec.Def && Effect->Spec.Def->GetClass() == EffectClass) ++Count;
	return Count;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGGASDerivedAttributesTest, "DuTraveller.GAS.DerivedAttributes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGGASDerivedAttributesTest::RunTest(const FString& Parameters)
{
	FGASWorld Context;
	ARPGPlayerState* PS = nullptr;
	ARPGCharacter* Pawn = BoundCharacter(Context, PS);
	const URPGAttributeSet* Stats = Pawn->GetRPGAttributes();
	UAbilitySystemComponent* ASC = Pawn->GetAbilitySystemComponent();
	TestTrue(TEXT("Pawn uses the PlayerState ASC"), ASC == PS->AbilitySystem.Get());
	TestEqual(TEXT("Derived derivation applied once"), CountEffectsOfClass(ASC, URPGDerivedAttributeEffect::StaticClass()), 1);
	TestEqual(TEXT("Resource regen applied once"), CountEffectsOfClass(ASC, URPGResourceRegenEffect::StaticClass()), 1);

	Pawn->Initialize(nullptr);
	TestEqual(TEXT("Re-init does not duplicate derivation"), CountEffectsOfClass(ASC, URPGDerivedAttributeEffect::StaticClass()), 1);
	TestEqual(TEXT("Re-init does not duplicate regen"), CountEffectsOfClass(ASC, URPGResourceRegenEffect::StaticClass()), 1);

	SetBase(Pawn, URPGAttributeSet::GetStrengthAttribute(), 10.f);
	TestEqual(TEXT("Strength derives attack power"), Stats->GetAttackPower(), 10.f);
	TestEqual(TEXT("Strength derives armor penetration"), Stats->GetArmorPenetration(), 2.5f);
	SetBase(Pawn, URPGAttributeSet::GetIntelligenceAttribute(), 8.f);
	TestEqual(TEXT("Intelligence derives ability power"), Stats->GetAbilityPower(), 8.f);
	TestEqual(TEXT("Intelligence derives magic penetration"), Stats->GetMagicPenetration(), 2.f);
	TestEqual(TEXT("Intelligence derives max mana"), Stats->GetMaxMana(), 90.f);
	SetBase(Pawn, URPGAttributeSet::GetVigorAttribute(), 5.f);
	TestEqual(TEXT("Vigor derives max health"), Stats->GetMaxHealth(), 150.f);
	TestEqual(TEXT("Vigor derives health regeneration"), Stats->GetHealthRegeneration(), .5f);
	SetBase(Pawn, URPGAttributeSet::GetDefenseAttribute(), 10.f);
	TestEqual(TEXT("Defense derives armor"), Stats->GetArmor(), 20.f);
	TestEqual(TEXT("Defense derives block chance"), Stats->GetBlockChance(), 5.f);
	SetBase(Pawn, URPGAttributeSet::GetResilienceAttribute(), 6.f);
	TestEqual(TEXT("Resilience derives magic crit resistance"), Stats->GetMagicCriticalHitResistance(), 3.f);
	SetBase(Pawn, URPGAttributeSet::GetLuckAttribute(), 10.f);
	TestEqual(TEXT("Luck derives crit chance"), Stats->GetCriticalHitChance(), 5.f);
	TestEqual(TEXT("Luck derives crit damage on top of the 1.5 default"), Stats->GetCriticalHitDamage(), 1.6f);

	// Attribute-point investment must flow through the same derivation.
	PS->GrantProgressionPoints(1, 0);
	const float AttackBefore = Stats->GetAttackPower();
	TestTrue(TEXT("Point purchase succeeds"), PS->TryUpgradeAttribute(FGameplayTag::RequestGameplayTag(FName("Attributes.Primary.Strength"))));
	TestEqual(TEXT("Invested point raises derived attack power"), Stats->GetAttackPower(), AttackBefore + 1.f);

	// Health/mana/stamina are replicated attributes; secondaries must also replicate.
	for (const FName Name : {FName("Health"), FName("MaxHealth"), FName("Mana"), FName("Stamina"), FName("AttackPower"), FName("Armor")})
		if (const FProperty* Property = FindFProperty<FProperty>(URPGAttributeSet::StaticClass(), Name))
			TestTrue(Name.ToString() + TEXT(" is a replicated attribute"), Property->HasAnyPropertyFlags(CPF_Net));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGGASAbilityActivationTest, "DuTraveller.GAS.AbilityActivationCommit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGGASAbilityActivationTest::RunTest(const FString& Parameters)
{
	FGASWorld Context;
	ARPGPlayerState* PS = nullptr;
	ARPGCharacter* Pawn = BoundCharacter(Context, PS);
	URPGAbilitySystemComponent* ASC = Cast<URPGAbilitySystemComponent>(Pawn->GetAbilitySystemComponent());
	if (!TestNotNull(TEXT("Player owns the DuTraveller ASC"), ASC)) return false;

	SetBase(Pawn, URPGAttributeSet::GetMaxManaAttribute(), 100.f);
	SetBase(Pawn, URPGAttributeSet::GetManaAttribute(), 100.f);
	SetBase(Pawn, URPGAttributeSet::GetMaxStaminaAttribute(), 100.f);
	SetBase(Pawn, URPGAttributeSet::GetStaminaAttribute(), 100.f);
	URPGCombatAbilityDefinition* Ability = MakeAbility(Pawn, TEXT("Abilities.Fire.FireBolt"), ERPGAbilityKind::Spell, 0.f);
	Ability->bRequiresTarget = false;
	Ability->MaximumRank = 3;
	Ability->AttackPowerScale = 0.f;
	Ability->ManaCost = 15.f;
	Ability->Cooldown = 2.f;
	const FGameplayTag InputTag = FGameplayTag::RequestGameplayTag(TEXT("InputTag.01"));

	const FGameplayAbilitySpecHandle Handle = ASC->EnsureAbilityGranted(Ability, 2, InputTag);
	TestTrue(TEXT("Authority grants a real ability spec"), Handle.IsValid());
	const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
	TestNotNull(TEXT("Granted spec is stored by the ASC"), Spec);
	if (Spec)
	{
		TestTrue(TEXT("Spec uses the GameplayAbility implementation"), Spec->Ability->IsA<URPGGameplayAbility>());
		TestTrue(TEXT("Spec retains its combat definition source"), Spec->SourceObject.Get() == Ability);
		TestEqual(TEXT("Learned rank becomes the GAS spec level"), Spec->Level, 2);
	}

	const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(TEXT("State.Dead"));
	ASC->SetLooseGameplayTagCount(DeadTag, 1);
	TestFalse(TEXT("GAS dead state blocks ability activation"), ASC->ActivateAbilityByInputTag(InputTag, nullptr));
	TestEqual(TEXT("Blocked state does not pay ability cost"), Pawn->GetRPGAttributes()->GetMana(), 100.f);
	ASC->SetLooseGameplayTagCount(DeadTag, 0);
	TestTrue(TEXT("Input tag activates the real ability after state clears"), ASC->ActivateAbilityByInputTag(InputTag, nullptr));
	TestEqual(TEXT("CommitAbility pays mana through the cost Gameplay Effect"), Pawn->GetRPGAttributes()->GetMana(), 85.f);
	TestTrue(TEXT("CommitAbility applies the authored cooldown"), Pawn->ActionComp->GetRemainingCooldown(Ability->AbilityId) > 0.f);
	Spec = ASC->FindAbilitySpecFromHandle(Handle);
	TestTrue(TEXT("Instant combat execution ends its ability instance"), Spec && !Spec->IsActive());
	const float ManaAfterCommit = Pawn->GetRPGAttributes()->GetMana();
	TestFalse(TEXT("Cooldown blocks immediate reactivation"), ASC->ActivateAbilityByInputTag(InputTag, nullptr));
	TestEqual(TEXT("Rejected activation does not pay cost again"), Pawn->GetRPGAttributes()->GetMana(), ManaAfterCommit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGGASPhysicalAttributesTest, "DuTraveller.GAS.PhysicalDamageAttributes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGGASPhysicalAttributesTest::RunTest(const FString& Parameters)
{
	FGASWorld Context;
	ARPGPlayerState* PS = nullptr;
	ARPGCharacter* Source = BoundCharacter(Context, PS);
	ARPGCharacter* Target = Context.Character();
	Target->FactionId = FName("Enemy");
	Target->SetActorLocation(FVector(200, 0, 0));
	ResetVitals(Target);
	URPGCombatAbilityDefinition* Slash = MakeAbility(Source, TEXT("Test.PhysicalStrike"), ERPGAbilityKind::Melee, 20.f);

	const auto Hit = [&](const FString& Label, const float ExpectedDamage)
	{
		const float Before = Target->GetRPGAttributes()->GetHealth();
		const bool bOk = Source->ActionComp->TryActivateAbility(Slash, 1, Target);
		const float Dealt = Before - Target->GetRPGAttributes()->GetHealth();
		TestTrue(Label + TEXT(" applies"), bOk && Dealt > 0.f);
		TestEqual(Label + TEXT(" damage"), static_cast<double>(Dealt), static_cast<double>(ExpectedDamage), .51);
	};

	Hit(TEXT("Baseline physical hit"), 20.f);
	const URPGAbilitySystemComponent* SourceASC = Cast<URPGAbilitySystemComponent>(Source->GetAbilitySystemComponent());
	TestTrue(TEXT("Compatibility activation grants a real Gameplay Ability spec"),
		SourceASC && SourceASC->HasGrantedAbility(Slash->AbilityId));
	SetBase(Source, URPGAttributeSet::GetAttackPowerAttribute(), 30.f);
	Hit(TEXT("AttackPower raises physical damage"), 50.f);
	SetBase(Target, URPGAttributeSet::GetArmorAttribute(), 100.f);
	Hit(TEXT("Armor mitigates physical damage"), 25.f);
	SetBase(Source, URPGAttributeSet::GetArmorPenetrationAttribute(), 50.f);
	Hit(TEXT("ArmorPenetration bypasses part of armor"), 33.33f);
	SetBase(Source, URPGAttributeSet::GetArmorPenetrationAttribute(), 0.f);
	SetBase(Target, URPGAttributeSet::GetArmorAttribute(), 0.f);
	SetBase(Target, URPGAttributeSet::GetDefenseAttribute(), 50.f);
	{
		const float Before = Target->GetRPGAttributes()->GetHealth();
		TestTrue(TEXT("Defense-armored strike applies"), Source->ActionComp->TryActivateAbility(Slash, 1, Target));
		const float Dealt = Before - Target->GetRPGAttributes()->GetHealth();
		// Defense derives +100 Armor (halves to 25) and +25% BlockChance (halves again when blocked).
		TestTrue(TEXT("Defense-derived armor mitigates physical damage"), Dealt == 25.f || Dealt == 12.5f);
	}
	SetBase(Target, URPGAttributeSet::GetBlockChanceAttribute(), 100.f);
	Hit(TEXT("Guaranteed block halves physical damage"), 12.5f);
	SetBase(Target, URPGAttributeSet::GetBlockChanceAttribute(), 0.f);
	SetBase(Target, URPGAttributeSet::GetDefenseAttribute(), 0.f);
	SetBase(Source, URPGAttributeSet::GetCriticalHitChanceAttribute(), 100.f);
	SetBase(Source, URPGAttributeSet::GetCriticalHitDamageAttribute(), 2.f);
	Hit(TEXT("Guaranteed crit multiplies physical damage"), 100.f);
	SetBase(Target, URPGAttributeSet::GetResilienceAttribute(), 400.f);
	Hit(TEXT("Resilience suppresses physical crits"), 50.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGGASMagicalAttributesTest, "DuTraveller.GAS.MagicalDamageAttributes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGGASMagicalAttributesTest::RunTest(const FString& Parameters)
{
	FGASWorld Context;
	ARPGPlayerState* PS = nullptr;
	ARPGCharacter* Source = BoundCharacter(Context, PS);
	ARPGCharacter* Target = Context.Character();
	Target->FactionId = FName("Enemy");
	Target->SetActorLocation(FVector(200, 0, 0));
	ResetVitals(Target);
	URPGCombatAbilityDefinition* Bolt = MakeAbility(Source, TEXT("Test.MagicBolt"), ERPGAbilityKind::Spell, 20.f);
	Bolt->AbilityPowerScale = 1.f;
	Bolt->AttackPowerScale = 0.f;

	const auto Hit = [&](const FString& Label, const float ExpectedDamage)
	{
		const float Before = Target->GetRPGAttributes()->GetHealth();
		const bool bOk = Source->ActionComp->TryActivateAbility(Bolt, 1, Target);
		const float Dealt = Before - Target->GetRPGAttributes()->GetHealth();
		TestTrue(Label + TEXT(" applies"), bOk && Dealt > 0.f);
		TestEqual(Label + TEXT(" damage"), static_cast<double>(Dealt), static_cast<double>(ExpectedDamage), .51);
	};

	Hit(TEXT("Baseline spell hit"), 20.f);
	SetBase(Source, URPGAttributeSet::GetAbilityPowerAttribute(), 20.f);
	Hit(TEXT("AbilityPower raises spell damage"), 40.f);
	SetBase(Target, URPGAttributeSet::GetArmorAttribute(), 100.f);
	Hit(TEXT("Physical armor does not mitigate spells"), 40.f);
	SetBase(Target, URPGAttributeSet::GetResilienceAttribute(), 100.f);
	Hit(TEXT("Resilience mitigates spell damage"), 20.f);
	SetBase(Source, URPGAttributeSet::GetMagicPenetrationAttribute(), 50.f);
	Hit(TEXT("MagicPenetration bypasses part of resist"), 26.67f);
	SetBase(Source, URPGAttributeSet::GetMagicCriticalHitChanceAttribute(), 200.f);
	SetBase(Source, URPGAttributeSet::GetMagicCriticalHitDamageAttribute(), 2.f);
	Hit(TEXT("Guaranteed magic crit multiplies spell damage"), 53.33f);
	SetBase(Source, URPGAttributeSet::GetMagicCriticalHitChanceAttribute(), 100.f);
	SetBase(Target, URPGAttributeSet::GetMagicCriticalHitResistanceAttribute(), 150.f);
	Hit(TEXT("Magic crit resistance suppresses spell crits"), 26.67f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGGASEquipmentDamageTest, "DuTraveller.GAS.EquipmentChangesRealDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGGASEquipmentDamageTest::RunTest(const FString& Parameters)
{
	FGASWorld Context;
	ARPGPlayerState* PS = nullptr;
	ARPGCharacter* Source = BoundCharacter(Context, PS);
	ARPGCharacter* Target = Context.Character();
	Target->FactionId = FName("Enemy");
	Target->SetActorLocation(FVector(200, 0, 0));
	ResetVitals(Target);
	URPGCombatAbilityDefinition* Slash = MakeAbility(Source, TEXT("Test.GearStrike"), ERPGAbilityKind::Melee, 20.f);

	Source->InventoryComp->TryAddItemFromClass(UGear_ItemObject::StaticClass(), 1);
	UGear_ItemObject* Weapon = CastChecked<UGear_ItemObject>(Source->InventoryComp->GetItem(0));
	Weapon->Slot = EEquipableSlot::EIS_PrimaryWeapon;
	Weapon->AttributeBonuses.Add(FName("AttackPower"), 25.f);
	TestTrue(TEXT("Weapon equips"), Source->TryEquipItem(Weapon, Weapon->Slot));
	TestEqual(TEXT("Gear raises GAS attack power"), Source->GetRPGAttributes()->GetAttackPower(), 25.f);
	const float Before = Target->GetRPGAttributes()->GetHealth();
	TestTrue(TEXT("Strike lands"), Source->ActionComp->TryActivateAbility(Slash, 1, Target));
	TestEqual(TEXT("Gear bonus reaches real combat damage"), static_cast<double>(Before - Target->GetRPGAttributes()->GetHealth()), 45., .51);

	TestTrue(TEXT("Weapon unequips"), Source->TryUnequipItem(Weapon->Slot));
	TestEqual(TEXT("Unequip removes the GAS bonus"), Source->GetRPGAttributes()->GetAttackPower(), 0.f);
	const float BeforePlain = Target->GetRPGAttributes()->GetHealth();
	TestTrue(TEXT("Strike lands without gear"), Source->ActionComp->TryActivateAbility(Slash, 1, Target));
	TestEqual(TEXT("Damage returns to the unmodified base"), static_cast<double>(BeforePlain - Target->GetRPGAttributes()->GetHealth()), 20., .51);

	Target->InventoryComp->TryAddItemFromClass(UGear_ItemObject::StaticClass(), 1);
	UGear_ItemObject* Plate = CastChecked<UGear_ItemObject>(Target->InventoryComp->GetItem(0));
	Plate->Slot = EEquipableSlot::EIS_Chest;
	Plate->AttributeBonuses.Add(FName("Armor"), 100.f);
	TestTrue(TEXT("Armor gear equips"), Target->TryEquipItem(Plate, Plate->Slot));
	const float BeforeArmored = Target->GetRPGAttributes()->GetHealth();
	TestTrue(TEXT("Strike lands on armored target"), Source->ActionComp->TryActivateAbility(Slash, 1, Target));
	TestEqual(TEXT("Equipped armor halves the real damage"), static_cast<double>(BeforeArmored - Target->GetRPGAttributes()->GetHealth()), 10., .51);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGGASLifecycleTest, "DuTraveller.GAS.DamageHealingDeathRespawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGGASLifecycleTest::RunTest(const FString& Parameters)
{
	FGASWorld Context;
	ARPGCharacter* Pawn = Context.Character();
	const URPGAttributeSet* Stats = Pawn->GetRPGAttributes();
	UAbilitySystemComponent* ASC = Pawn->GetAbilitySystemComponent();

	TestTrue(TEXT("Combat damage enters through GAS"), Pawn->ReceiveCombatDamage(40.f, nullptr, TEXT("Test.Damage")));
	TestEqual(TEXT("GAS health drops"), Stats->GetHealth(), 60.f);
	TestEqual(TEXT("Legacy mirror follows GAS health"), Pawn->AttributeComp->Health, 60.f);

	SetBase(Pawn, URPGAttributeSet::GetHealthAttribute(), 50.f);
	URPGSkillExecution::RestoreResources(Pawn, 100.f, 0.f);
	TestEqual(TEXT("Healing clamps at MaxHealth"), Stats->GetHealth(), 100.f);
	SetBase(Pawn, URPGAttributeSet::GetHealthAttribute(), 50.f);
	SetBase(Pawn, URPGAttributeSet::GetManaAttribute(), 30.f);
	URPGSkillExecution::RestoreResources(Pawn, 25.f, 10.f);
	TestEqual(TEXT("Healing restores through a Gameplay Effect"), Stats->GetHealth(), 75.f);
	TestEqual(TEXT("Mana restores through the same effect"), Stats->GetMana(), 40.f);

	// The periodic upkeep GE is registered with a real period; its attribute-sourced
	// magnitudes keep regeneration inside the GAS clamps verified above.
	bool bRegenPeriodic = false;
	for (const FActiveGameplayEffectHandle& Handle : ASC->GetActiveEffects(FGameplayEffectQuery()))
		if (const FActiveGameplayEffect* Effect = ASC->GetActiveGameplayEffect(Handle))
			if (Effect->Spec.Def && Effect->Spec.Def->IsA<URPGResourceRegenEffect>())
				bRegenPeriodic |= Effect->GetPeriod() > 0.f;
	TestTrue(TEXT("Regeneration is an active periodic GAS effect"), bRegenPeriodic);
	SetBase(Pawn, URPGAttributeSet::GetHealthAttribute(), 50.f);
	SetBase(Pawn, URPGAttributeSet::GetHealthRegenerationAttribute(), 5.f);
	FGameplayEffectSpec RegenTick(GetDefault<URPGConsumableResourceEffect>(), ASC->MakeEffectContext(), 1.f);
	RegenTick.SetSetByCallerMagnitude(FName("Health"), Stats->GetHealthRegeneration());
	RegenTick.SetSetByCallerMagnitude(FName("Mana"), 0.f);
	RegenTick.SetSetByCallerMagnitude(FName("Stamina"), 0.f);
	ASC->ApplyGameplayEffectSpecToSelf(RegenTick);
	TestEqual(TEXT("A regeneration tick raises GAS health"), Stats->GetHealth(), 55.f);
	FGameplayEffectSpec CappedTick(GetDefault<URPGConsumableResourceEffect>(), ASC->MakeEffectContext(), 1.f);
	CappedTick.SetSetByCallerMagnitude(FName("Health"), 500.f);
	CappedTick.SetSetByCallerMagnitude(FName("Mana"), 0.f);
	CappedTick.SetSetByCallerMagnitude(FName("Stamina"), 0.f);
	ASC->ApplyGameplayEffectSpecToSelf(CappedTick);
	TestEqual(TEXT("Regeneration never exceeds MaxHealth"), Stats->GetHealth(), 100.f);

	TestTrue(TEXT("Lethal damage applies"), Pawn->ReceiveCombatDamage(5000.f, nullptr, TEXT("Test.Fatal")));
	TestTrue(TEXT("GAS zero health marks death"), Pawn->bIsDead);
	const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(TEXT("State.Dead"));
	TestTrue(TEXT("Death publishes State.Dead through the ASC"), ASC->HasMatchingGameplayTag(DeadTag));
	TestFalse(TEXT("Dead characters reject further damage"), Pawn->ReceiveCombatDamage(1.f, nullptr, TEXT("Test.Fatal")));
	TestTrue(TEXT("Respawn restores through GAS"), Pawn->RespawnAt(FTransform(FVector(100, 0, 0))));
	TestFalse(TEXT("Respawn clears death"), Pawn->bIsDead);
	TestFalse(TEXT("Respawn clears State.Dead"), ASC->HasMatchingGameplayTag(DeadTag));
	TestEqual(TEXT("Respawn refills GAS health"), Stats->GetHealth(), Stats->GetMaxHealth());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGGASInvulnerabilityTest, "DuTraveller.GAS.InvulnerabilityGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGGASInvulnerabilityTest::RunTest(const FString& Parameters)
{
	FGASWorld Context;
	ARPGCharacter* Victim = Context.Character();
	UAbilitySystemComponent* ASC = Victim->GetAbilitySystemComponent();
	const FGameplayTag Invulnerable = FGameplayTag::RequestGameplayTag(FName("State.Invulnerable"), false);
	const FGameplayTag Dodging = FGameplayTag::RequestGameplayTag(FName("State.Dodging"), false);
	TestTrue(TEXT("Invulnerability tag is registered"), Invulnerable.IsValid());
	TestTrue(TEXT("Dodging tag is registered"), Dodging.IsValid());

	ASC->SetLooseGameplayTagCount(Invulnerable, 1);
	TestFalse(TEXT("Tagged target rejects combat damage"), Victim->ReceiveCombatDamage(10.f, nullptr, TEXT("Test.Invuln")));
	TestEqual(TEXT("Health untouched while tagged"), Victim->GetRPGAttributes()->GetHealth(), 100.f);
	ASC->SetLooseGameplayTagCount(Invulnerable, 0);
	TestTrue(TEXT("Damage works immediately after the tag clears"), Victim->ReceiveCombatDamage(10.f, nullptr, TEXT("Test.Invuln")));
	TestEqual(TEXT("Health drops after immunity ends"), Victim->GetRPGAttributes()->GetHealth(), 90.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGDodgeAuthorityTest, "DuTraveller.Dodge.AuthorityAndTags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGDodgeAuthorityTest::RunTest(const FString& Parameters)
{
	const ARPGPlayerCharacter* Defaults = GetDefault<ARPGPlayerCharacter>();
	TestEqual(TEXT("Native roll duration is one second"), Defaults->DodgeDuration, 1.f);
	TestEqual(TEXT("Native roll distance is 500 units"), Defaults->DodgeDistance, 500.f);
	TestTrue(TEXT("Full-animation immunity enabled"), Defaults->bDodgeInvulnerability);

	UClass* PlayerClass = LoadClass<ARPGCharacter>(nullptr,
		TEXT("/Game/Blueprints/Characters/BP_PlayerCharacter.BP_PlayerCharacter_C"));
	if (!TestNotNull(TEXT("Player Blueprint loads"), PlayerClass)) return false;
	const ARPGPlayerCharacter* PlayerDefaults = CastChecked<ARPGPlayerCharacter>(PlayerClass->GetDefaultObject());
	TestEqual(TEXT("Serialized roll duration"), PlayerDefaults->DodgeDuration, 1.f);
	TestEqual(TEXT("Serialized roll distance"), PlayerDefaults->DodgeDistance, 500.f);
	TestNotNull(TEXT("Roll montage is authored"), PlayerDefaults->DodgeAnimation.Montage.Get());

	FGASWorld Context(true);
	ARPGPlayerCharacter* Hero = Cast<ARPGPlayerCharacter>(Context.World->SpawnActor<ARPGCharacter>(PlayerClass, FVector(0, 0, 90), FRotator::ZeroRotator));
	if (!TestNotNull(TEXT("Player character spawns"), Hero)) return false;
	Hero->Initialize(nullptr);
	Hero->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	UAbilitySystemComponent* ASC = Hero->GetAbilitySystemComponent();
	const FGameplayTag Invulnerable = FGameplayTag::RequestGameplayTag(FName("State.Invulnerable"), false);
	const FGameplayTag Dodging = FGameplayTag::RequestGameplayTag(FName("State.Dodging"), false);

	const FVector Start = Hero->GetActorLocation();
	const float StaminaBefore = Hero->GetRPGAttributes()->GetStamina();
	TestTrue(TEXT("Roll request is accepted"), Hero->RequestDodge(FVector::ForwardVector));
	TestTrue(TEXT("Dodge state active"), Hero->bIsDodging);
	TestTrue(TEXT("State.Dodging applied"), ASC->HasMatchingGameplayTag(Dodging));
	TestTrue(TEXT("State.Invulnerable applied"), ASC->HasMatchingGameplayTag(Invulnerable));
	TestTrue(TEXT("GAS stamina paid"), Hero->GetRPGAttributes()->GetStamina() < StaminaBefore);
	TestFalse(TEXT("Damage rejected mid-roll"), Hero->ReceiveCombatDamage(15.f, nullptr, TEXT("Test.Roll")));

	const auto StepDodge = [&](const float Seconds)
	{
		float Elapsed = 0.f;
		while (Elapsed < Seconds)
		{
			const float Delta = FMath::Min(.05f, Seconds - Elapsed);
			Hero->Tick(Delta);
			if (USkeletalMeshComponent* Mesh = Hero->GetMesh()) Mesh->TickComponent(Delta, ELevelTick::LEVELTICK_All, nullptr);
			if (UCharacterMovementComponent* Move = Hero->GetCharacterMovement()) Move->TickComponent(Delta, ELevelTick::LEVELTICK_All, nullptr);
			Elapsed += Delta;
		}
	};
	StepDodge(.5f);
	TestTrue(TEXT("Immunity still active halfway through the roll"), ASC->HasMatchingGameplayTag(Invulnerable));
	TestFalse(TEXT("Damage still rejected halfway through"), Hero->ReceiveCombatDamage(15.f, nullptr, TEXT("Test.Roll")));

	StepDodge(1.4f);
	TestFalse(TEXT("Roll finishes after roughly one second"), Hero->bIsDodging);
	TestFalse(TEXT("Invulnerability ends with the roll"), ASC->HasMatchingGameplayTag(Invulnerable));
	TestFalse(TEXT("Dodge tag ends with the roll"), ASC->HasMatchingGameplayTag(Dodging));
	const float Traveled = (Hero->GetActorLocation() - Start).Size2D();
	const bool bAuthoredMotion = Hero->DodgeAnimation.Montage && Hero->DodgeAnimation.Montage->HasRootMotion();
	if (bAuthoredMotion)
	{
		// Montage root motion needs the animation update graph, which does not run
		// in a headless automation world. The authored 500uu displacement is covered
		// by the live PIE checks in Run-DodgeFocusPIE / Test-DodgeMultiplayer.
		AddInfo(FString::Printf(TEXT("Authored root motion roll: headless travel %.1f, live PIE validates the 500uu displacement"), Traveled));
	}
	else
	{
		TestTrue(TEXT("Capsule travels the authored distance"), Traveled > 400.f);
		if (Traveled > 400.f) TestTrue(TEXT("No snapback short of the authored distance"), Traveled > 475.f);
	}
	TestTrue(TEXT("Damage works immediately after the roll"), Hero->ReceiveCombatDamage(15.f, nullptr, TEXT("Test.PostRoll")));
	return true;
}

#endif
