#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "../RPGCharacter.h"
#include "../RPGPlayerState.h"
#include "../RPGAttributeSet.h"
#include "../AttributeComponent.h"
#include "../InventoryComponent.h"
#include "../Gear_ItemObject.h"
#include "../Food_ItemObject.h"
#include "../Potion_ItemObject.h"
#include "../Currency_ItemObject.h"
#include "../RPGCraftingComponent.h"
#include "../ItemBlueprint.h"
#include "../RPGDynamicQuestComponent.h"
#include "../RPGRandomQuestSpawner.h"
#include "../RPGSpawnQuestSaveGame.h"
#include "../RPGPickupItem.h"
#include "../RPGVendor.h"
#include "../RPGCombatTypes.h"
#include "../RPGActionComponent.h"
#include "../RPGAbilitySystemComponent.h"
#include "../RPGSkillComponent.h"
#include "../RPGPartyComponent.h"
#include "../RPGAICharacter.h"
#include "../RPGPlayerCharacter.h"
#include "../RPGNPCCharacter.h"
#include "../RPGQuestGiver.h"
#include "../RPGMountCharacter.h"
#include "../RPGAIController.h"
#include "../RPGGuildState.h"
#include "../RPGBuilding.h"
#include "../RPG_ItemToolTip.h"
#include "../RPG_ItemWidget.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "../RPGPlayerController.h"
#include "../RPGProjectile.h"
#include "../RPGSkillExecution.h"
#include "../DTAnimationRouter.h"
#include "InputKeyEventArgs.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "EngineUtils.h"
#include "Animation/AnimMontage.h"
#include "../DTInventoryDragDrop.h"
#include "../RPG_UserWidget.h"
#include "../RPGOverlayWidgetController.h"
#include "../DTStructRepairLibrary.h"

namespace
{
struct FRestorationWorld
{
	UWorld* World;
	explicit FRestorationWorld(bool bPhysics = false)
	{
		const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
			.CreatePhysicsScene(bPhysics).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		// Spawned actors must run component initialization so GAS discovers
		// default attribute subobjects, as it does in a real gameplay world.
		World->InitializeActorsForPlay(FURL());
	}
	~FRestorationWorld()
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
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGInvestmentEquipmentTest, "DuTraveller.Restoration.InvestmentAndEquipment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGInvestmentEquipmentTest::RunTest(const FString& Parameters)
{
	FRestorationWorld Context;
	ARPGPlayerState* PS = Context.World->SpawnActor<ARPGPlayerState>();
	ARPGCharacter* Character = Context.Character();
	Character->SetPlayerState(PS);
	Character->Initialize(nullptr);
	PS->AbilitySystem->SetNumericAttributeBase(URPGAttributeSet::GetStrengthAttribute(), 10.f);
	const FGameplayTag Strength = FGameplayTag::RequestGameplayTag(FName("Attributes.Primary.Strength"));
	TestTrue(TEXT("Server awards separate point balances"), PS->GrantProgressionPoints(2, 3));
	TestTrue(TEXT("First permanent investment succeeds"), PS->TryUpgradeAttribute(Strength));
	TestEqual(TEXT("Permanent base increased"), PS->Attributes->GetStrength(), 11.f);
	TestEqual(TEXT("Spell points unaffected"), PS->SpellPoints, 3);

	Character->InventoryComp->TryAddItemFromClass(UGear_ItemObject::StaticClass(), 2);
	UGear_ItemObject* First = CastChecked<UGear_ItemObject>(Character->InventoryComp->GetItem(0));
	UGear_ItemObject* Second = CastChecked<UGear_ItemObject>(Character->InventoryComp->GetItem(1));
	First->Slot = EEquipableSlot::EIS_Helmet;
	First->AttributeBonuses.Add(FName("Strength"), 5.f);
	First->AttributeBonuses.Add(FName("MaxHealth"), 50.f);
	Second->Slot = EEquipableSlot::EIS_Helmet;
	Second->AttributeBonuses.Add(FName("Strength"), 9.f);
	Second->AttributeBonuses.Add(FName("MaxHealth"), 50.f);
	TestTrue(TEXT("Equip owned compatible gear"), Character->TryEquipItem(First, EEquipableSlot::EIS_Helmet));
	TestTrue(TEXT("Inventory use toggles equipped gear off"), Character->InventoryComp->UseItem(First));
	TestFalse(TEXT("Use unequipped it"), Character->IsItemEquipped(First));
	TestTrue(TEXT("Inventory use equips gear"), Character->InventoryComp->UseItem(First));
	TestTrue(TEXT("Equipment remains owned and unconsumed"), Character->InventoryComp->FindItem(First) == First && First->GetQuantity() == 1);
	TestEqual(TEXT("Gear adds to invested base"), PS->Attributes->GetStrength(), 16.f);
	PS->AbilitySystem->SetNumericAttributeBase(URPGAttributeSet::GetHealthAttribute(), 145.f);
	TestTrue(TEXT("Repeated equip accepted without duplicate bonus"), Character->TryEquipItem(First, EEquipableSlot::EIS_Helmet));
	TestEqual(TEXT("Repeated equip does not stack"), PS->Attributes->GetStrength(), 16.f);
	TestEqual(TEXT("Repeated equip preserves current health"), PS->Attributes->GetHealth(), 145.f);
	TestTrue(TEXT("Swap succeeds"), Character->TryEquipItem(Second, EEquipableSlot::EIS_Helmet));
	TestEqual(TEXT("Old bonus replaced"), PS->Attributes->GetStrength(), 20.f);
	TestEqual(TEXT("Same-max-health swap preserves health"), PS->Attributes->GetHealth(), 145.f);
	TestTrue(TEXT("Investment while geared succeeds"), PS->TryUpgradeAttribute(Strength));
	TestEqual(TEXT("Base and gear compose"), PS->Attributes->GetStrength(), 21.f);
	TestFalse(TEXT("Insufficient points rejected"), PS->TryUpgradeAttribute(Strength));
	TestFalse(TEXT("Cannot remove worn item from inventory"), Character->InventoryComp->RemoveItem(Second));
	TestTrue(TEXT("Unequip succeeds"), Character->TryUnequipItem(EEquipableSlot::EIS_Helmet));
	TestEqual(TEXT("Both permanent purchases remain"), PS->Attributes->GetStrength(), 12.f);
	TestEqual(TEXT("Health respects reduced max"), PS->Attributes->GetHealth(), 100.f);
	TestEqual(TEXT("Attribute points deducted exactly twice"), PS->AttributePoints, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGCombatSkillTest, "DuTraveller.Restoration.CombatAndHybridSkills",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGCombatSkillTest::RunTest(const FString& Parameters)
{
	FRestorationWorld Context;
	ARPGPlayerState* PS = Context.World->SpawnActor<ARPGPlayerState>();
	ARPGCharacter* Source = Context.Character();
	ARPGCharacter* Target = Context.Character();
	Source->SetPlayerState(PS); Source->Initialize(nullptr);
	Target->FactionId = FName("Enemy");
	Source->SetActorLocation(FVector::ZeroVector); Target->SetActorLocation(FVector(100,0,0));
	PS->LVL = 3; PS->GrantProgressionPoints(0, 3);
	PS->AbilitySystem->SetNumericAttributeBase(URPGAttributeSet::GetAttackPowerAttribute(), 10.f);
	PS->AbilitySystem->SetNumericAttributeBase(URPGAttributeSet::GetStaminaAttribute(), 20.f);
	Target->GetAbilitySystemComponent()->SetNumericAttributeBase(URPGAttributeSet::GetArmorAttribute(), 100.f);
	URPGCombatAbilityDefinition* Strike = NewObject<URPGCombatAbilityDefinition>(PS->Skills);
	Strike->AbilityId = FName("Skill.Strike"); Strike->MaximumRank = 2; Strike->SpellPointCostPerRank = 1;
	Strike->BaseDamageByRank = {10.f, 20.f}; Strike->AttackPowerScale = 1.f; Strike->StaminaCost = 5.f;
	Strike->Range = 200.f; Strike->Cooldown = 1.f;
	PS->Skills->AvailableAbilities.Add(Strike);
	TestTrue(TEXT("First skill rank learned"), PS->Skills->TryLearnAbility(Strike));
	TestTrue(TEXT("Same path can be upgraded"), PS->Skills->TryLearnAbility(Strike));
	TestEqual(TEXT("Rank and spell cost are authoritative"), PS->Skills->GetLearnedRank(Strike->AbilityId), 2);
	TestEqual(TEXT("Two spell points consumed"), PS->SpellPoints, 1);
	TestTrue(TEXT("Learned skill equips to hotbar"), PS->Skills->TryEquipHotbar(Strike, 0));
	TestTrue(TEXT("Hotbar skill activates against enemy"), PS->Skills->TryActivateHotbar(0, Target));
	TestEqual(TEXT("Armor mitigation applied"), Target->GetRPGAttributes()->GetHealth(), 85.f);
	TestEqual(TEXT("Stamina cost applied once"), Source->GetRPGAttributes()->GetStamina(), 15.f);
	TestFalse(TEXT("Cooldown rejects immediate replay"), PS->Skills->TryActivateHotbar(0, Target));
	URPGCombatAbilityDefinition* OtherTree = NewObject<URPGCombatAbilityDefinition>(PS->Skills);
	OtherTree->AbilityId = FName("Spell.Bolt"); OtherTree->Kind = ERPGAbilityKind::Spell; OtherTree->SpellPointCostPerRank = 1;
	OtherTree->BaseDamageByRank = {200.f}; OtherTree->AttackPowerScale = 0.f; OtherTree->Range = 500.f; OtherTree->Cooldown = 0.f;
	PS->Skills->AvailableAbilities.Add(OtherTree);
	TestTrue(TEXT("Hybrid player can learn another tree"), PS->Skills->TryLearnAbility(OtherTree));
	TestTrue(TEXT("Second-tree skill equips"), PS->Skills->TryEquipHotbar(OtherTree, 1));
	TestTrue(TEXT("Lethal skill succeeds"), PS->Skills->TryActivateHotbar(1, Target));
	TestTrue(TEXT("Target enters replicated death state"), Target->bIsDead);
	TestEqual(TEXT("Lethal damage clamps health"), Target->GetRPGAttributes()->GetHealth(), 0.f);
	TestFalse(TEXT("Dead target cannot be damaged repeatedly"), PS->Skills->TryActivateHotbar(1, Target));
	Source->FactionId = FName("Enemy");
	ARPGCharacter* Ally = Context.Character(); Ally->FactionId = FName("Enemy"); Ally->SetActorLocation(FVector(50,0,0));
	TestFalse(TEXT("Friendly fire is rejected by default"), Source->ActionComp->TryActivateAbility(OtherTree, 1, Ally));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGStarterTravelProgressTest, "DuTraveller.Restoration.StarterSkillsAndTravelProgress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGStarterTravelProgressTest::RunTest(const FString& Parameters)
{
	FRestorationWorld Context;
	ARPGPlayerState* Source = Context.World->SpawnActor<ARPGPlayerState>();
	ARPGPlayerState* Target = Context.World->SpawnActor<ARPGPlayerState>();
	URPGCombatAbilityDefinition* Skill = NewObject<URPGCombatAbilityDefinition>(Source->Skills);
	Skill->AbilityId = TEXT("Skill.TravelProgress"); Skill->MaximumRank = 3;
	Skill->BaseDamageByRank = {10.f, 20.f, 30.f};
	Source->Skills->AvailableAbilities.Add(Skill);
	Target->Skills->AvailableAbilities.Add(Skill);
	FRPGHotbarSlot Starter; Starter.Slot = 0; Starter.Definition = Skill;
	Target->Skills->StarterHotbar.Add(Starter);
	Source->DispatchBeginPlay(); Target->DispatchBeginPlay();
	TestEqual(TEXT("Starter loadout exists before travel copy"), Target->Skills->GetLearnedRank(Skill->AbilityId), 1);
	TestTrue(TEXT("Source learns rank one"), Source->Skills->TryLearnAbility(Skill));
	TestTrue(TEXT("Source learns rank two"), Source->Skills->TryLearnAbility(Skill));
	TestTrue(TEXT("Source equips its chosen slot"), Source->Skills->TryEquipHotbar(Skill, 4));
	Source->CopyProperties(Target);
	TestEqual(TEXT("Copied rank replaces fresh starter rank"), Target->Skills->GetLearnedRank(Skill->AbilityId), 2);
	TestEqual(TEXT("Travel retains skill points"), Target->SpellPoints, Source->SpellPoints);
	TestEqual(TEXT("Travel retains attribute points"), Target->AttributePoints, Source->AttributePoints);
	TestEqual(TEXT("Only the source loadout remains"), Target->Skills->Hotbar.Num(), 1);
	if (Target->Skills->Hotbar.Num() == 1) TestEqual(TEXT("Chosen slot survives travel"), Target->Skills->Hotbar[0].Slot, 4);
	Source->CopyProperties(Target);
	TestEqual(TEXT("Repeated state copy does not duplicate skills"), Target->Skills->LearnedAbilities.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGGameplayCatalogueTest, "DuTraveller.Restoration.GameplayDataCatalogues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGGameplayCatalogueTest::RunTest(const FString& Parameters)
{
	FRestorationWorld Context;
	ARPGPlayerState* PS = Context.World->SpawnActor<ARPGPlayerState>();
	URPGCombatAbilityDefinition* Skill = NewObject<URPGCombatAbilityDefinition>();
	Skill->AbilityId = TEXT("Skill.FromTable"); Skill->BaseDamageByRank = {12.f};
	UDataTable* Skills = NewObject<UDataTable>(); Skills->RowStruct = FRPGAbilityCatalogueRow::StaticStruct();
	FRPGAbilityCatalogueRow SkillRow; SkillRow.Definition = Skill; Skills->AddRow(Skill->AbilityId, SkillRow);
	PS->Skills->AbilityCatalogue = Skills; PS->GrantProgressionPoints(0, 1);
	TestTrue(TEXT("Skill table supplies the catalogue without inline definitions"), PS->Skills->GetAvailableDefinitions().Contains(Skill));
	TestTrue(TEXT("Table-backed skill can be learned"), PS->Skills->TryLearnAbility(Skill));
	TestTrue(TEXT("Table-backed skill can be equipped"), PS->Skills->TryEquipHotbar(Skill, 0));
	URPGCombatAbilityDefinition* Foreign = NewObject<URPGCombatAbilityDefinition>();
	Foreign->AbilityId = TEXT("Skill.NotInTable"); Foreign->BaseDamageByRank = {12.f};
	PS->Skills->AvailableAbilities.Add(Foreign);
	TestFalse(TEXT("Configured skill table prevents inline bypass"), PS->Skills->GetAvailableDefinitions().Contains(Foreign));
	UDataTable* Wrong = NewObject<UDataTable>(); Wrong->RowStruct = FRPGQuestCatalogueRow::StaticStruct();
	PS->Skills->AbilityCatalogue = Wrong;
	AddExpectedError(TEXT("Skill catalogue has incompatible row type"), EAutomationExpectedErrorFlags::Contains, 1);
	TestTrue(TEXT("Incompatible skill table fails closed"), PS->Skills->GetAvailableDefinitions().IsEmpty());
	PS->Skills->AbilityCatalogue = Skills;

	URPGRandomQuestDefinition* Quest = NewObject<URPGRandomQuestDefinition>();
	Quest->QuestId = TEXT("Quest.FromTable"); Quest->RandomRewardRolls = 0;
	FRPGDynamicObjective Objective; Objective.ObjectiveId = TEXT("Inspect"); Objective.Event = TEXT("Interacted");
	Quest->Objectives.Add(Objective);
	UDataTable* Quests = NewObject<UDataTable>(); Quests->RowStruct = FRPGQuestCatalogueRow::StaticStruct();
	FRPGQuestCatalogueRow QuestRow; QuestRow.Definition = Quest; QuestRow.Weight = 3; Quests->AddRow(Quest->QuestId, QuestRow);
	URPGQuestGroupDefinition* Group = NewObject<URPGQuestGroupDefinition>();
	Group->GroupId = TEXT("Group.FromTable"); Group->QuestCatalogue = Quests;
	FRPGWeightedQuest LegacyEntry; LegacyEntry.Definition = Quest; LegacyEntry.Weight = 99; Group->Quests.Add(LegacyEntry);
	const auto Entries = Group->GetQuestEntries();
	TestEqual(TEXT("Quest IDs are not duplicated across table and inline data"), Entries.Num(), 1);
	if (Entries.Num() == 1) TestEqual(TEXT("Quest table controls the selection weight"), Entries[0].Weight, 3);
	ARPGRandomQuestSpawner* Spawner = Context.World->SpawnActor<ARPGRandomQuestSpawner>();
	Spawner->AreaId = TEXT("Area.TableTest"); Spawner->LocationId = TEXT("Location.TableTest");
	Spawner->QuestGroups.Add(Group);
	TestTrue(TEXT("Authoritative spawner generates an offer from the quest table"), Spawner->GenerateOffer());
	if (!Spawner->Offers.IsEmpty()) TestTrue(TEXT("Published offer uses the table definition"), Spawner->Offers[0].Definition == Quest);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGCraftingCatalogueContentTest, "DuTraveller.Restoration.CraftingCatalogueContent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGCraftingCatalogueContentTest::RunTest(const FString& Parameters)
{
	FRestorationWorld Context;
	ARPGCharacter* Character = Context.Character();
	UDataTable* Catalogue = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/Catalogues/DT_CraftingRecipes.DT_CraftingRecipes"));
	if (!TestNotNull(TEXT("Authored crafting table is available"), Catalogue)) return false;
	Character->CraftingComp->RecipeCatalogue = Catalogue;
	TestTrue(TEXT("Authoritative component loads the recipe table"), Character->CraftingComp->InitializeRecipeCatalogue());
	const auto Recipes = Character->CraftingComp->GetRecipes();
	if (!TestEqual(TEXT("Starter catalogue provides one recipe"), Recipes.Num(), 1)) return false;
	UItemBlueprint* Recipe = Recipes[0];
	for (const FRPGInventoryAmount& Cost : Recipe->Ingredients) Character->InventoryComp->TryAddItemFromClass(Cost.ItemClass, Cost.Quantity);
	TestTrue(TEXT("Recipe consumes real ingredients and creates its output"), Character->CraftingComp->TryCraftRecipe(0));
	TestEqual(TEXT("Craft creates one potion"), Character->InventoryComp->CountItemsOfExactClass(Recipe->CraftedItem), int64(1));
	TestFalse(TEXT("Missing ingredients prevent a second craft"), Character->CraftingComp->TryCraftRecipe(0));
	TestTrue(TEXT("Transient test damage applies"), Character->ReceiveCombatDamage(30.f, Character, TEXT("Test.Crafting")));
	UItemObject* Potion = Character->InventoryComp->FindItemByClass(Recipe->CraftedItem);
	TestTrue(TEXT("Crafted potion applies its configured GAS effect"), Character->InventoryComp->UseItem(Potion));
	TestEqual(TEXT("Potion restores its authored 25 health"), Character->GetRPGAttributes()->GetHealth(), 95.f);
	TestEqual(TEXT("Used potion is consumed"), Character->InventoryComp->CountItemsOfExactClass(Recipe->CraftedItem), int64(0));
	Character->CraftingComp->InitializeRecipeCatalogue();
	TestEqual(TEXT("Repeated catalogue initialization does not duplicate recipes"), Character->CraftingComp->GetRecipes().Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGInventoryValidationTest, "DuTraveller.Restoration.InventoryValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGInventoryValidationTest::RunTest(const FString& Parameters)
{
	FRestorationWorld Context;
	ARPGCharacter* Owner = Context.Character();
	ARPGCharacter* Other = Context.Character();
	Owner->InventoryComp->Capacity = 1;
	const FItemAddResult Added = Owner->InventoryComp->TryAddItemFromClass(UItemObject::StaticClass(), 2);
	TestEqual(TEXT("Capacity limits addition"), Added.AmountGiven, 1);
	UItemObject* Item = Owner->InventoryComp->GetItem(0);
	TestTrue(TEXT("Stable item ID assigned"), Item->ItemId.IsValid());
	TestFalse(TEXT("Foreign inventory cannot consume"), Other->InventoryComp->ConsumeItem(Item, 1));
	TestFalse(TEXT("Negative consumption rejected"), Owner->InventoryComp->ConsumeItem(Item, -1));
	TestFalse(TEXT("Excess consumption rejected"), Owner->InventoryComp->ConsumeItem(Item, 2));
	TestFalse(TEXT("Item without a use effect is not consumed"), Owner->InventoryComp->UseItem(Item));
	TestEqual(TEXT("Failed operations preserve quantity"), Item->GetQuantity(), 1);
	Owner->DropItem(Item, 1);
	TestEqual(TEXT("Successful drop removes exactly one inventory item"), Owner->InventoryComp->Items.Num(), 0);
	TArray<AActor*> Pickups;
	UGameplayStatics::GetAllActorsOfClass(Context.World, ARPGPickupItem::StaticClass(), Pickups);
	TestEqual(TEXT("Drop creates one recoverable pickup"), Pickups.Num(), 1);
	if (ARPGPickupItem* Pickup = Pickups.Num() == 1 ? Cast<ARPGPickupItem>(Pickups[0]) : nullptr)
	{
		Pickup->SetActorLocation(Owner->GetActorLocation());
		TestTrue(TEXT("Nearby pickup returns dropped item"), Owner->CollectPickup(Pickup));
	}
	TestEqual(TEXT("Pickup restores exact inventory quantity"), Owner->InventoryComp->Items.Num(), 1);
	Item = Owner->InventoryComp->GetItem(0);
	TestEqual(TEXT("Foreign removal cannot steal item"), Other->InventoryComp->RemoveItem(Item), false);
	TestTrue(TEXT("Exact valid consumption succeeds"), Owner->InventoryComp->ConsumeItem(Item, 1));
	TestEqual(TEXT("Empty stack removed"), Owner->InventoryComp->Items.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGConsumableTest, "DuTraveller.Restoration.ConsumableEffects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGConsumableTest::RunTest(const FString& Parameters)
{
	FRestorationWorld Context;
	ARPGCharacter* Character = Context.Character();
	UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	ASC->SetNumericAttributeBase(URPGAttributeSet::GetHealthAttribute(), 50.f);
	Character->InventoryComp->TryAddItemFromClass(UFood_ItemObject::StaticClass(), 1);
	UFood_ItemObject* Food = CastChecked<UFood_ItemObject>(Character->InventoryComp->GetItem(0));
	TestFalse(TEXT("Unconfigured food preserves quantity"), Character->InventoryComp->UseItem(Food));
	Food->UseEffect = URPGConsumableResourceEffect::StaticClass();
	Food->SetByCallerMagnitudes.Add(FName("Health"), 25.f);
	TestTrue(TEXT("Food applies configured effect and consumes once"), Character->InventoryComp->UseItem(Food));
	TestEqual(TEXT("Health restored"), Character->GetRPGAttributes()->GetHealth(), 75.f);
	TestEqual(TEXT("Food removed after successful use"), Character->InventoryComp->Items.Num(), 0);
	TestFalse(TEXT("Removed item cannot be consumed again"), Character->InventoryComp->UseItem(Food));
	Character->InventoryComp->TryAddItemFromClass(UPotion_ItemObject::StaticClass(), 1);
	UPotion_ItemObject* Potion = CastChecked<UPotion_ItemObject>(Character->InventoryComp->GetItem(0));
	Potion->UseEffect = URPGConsumableResourceEffect::StaticClass();
	Potion->SetByCallerMagnitudes.Add(FName("Health"), 100.f);
	TestTrue(TEXT("Potion succeeds"), Character->InventoryComp->UseItem(Potion));
	TestEqual(TEXT("Restoration clamps to maximum"), Character->GetRPGAttributes()->GetHealth(), 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGTransactionsTest, "DuTraveller.Restoration.CraftingAndCurrencyTransactions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGTransactionsTest::RunTest(const FString& Parameters)
{
	FRestorationWorld Context;
	ARPGCharacter* Character = Context.Character();
	UInventoryComponent* Inventory = Character->InventoryComp;
	Inventory->Capacity = 2;
	Inventory->TryAddItemFromClass(UCurrency_ItemObject::StaticClass(), 20);
	FRPGInventoryAmount Price; Price.ItemClass = UCurrency_ItemObject::StaticClass(); Price.Quantity = 5;
	FRPGInventoryAmount Goods; Goods.ItemClass = UGear_ItemObject::StaticClass(); Goods.Quantity = 1;
	TestTrue(TEXT("Purchase commits both sides"), Inventory->TryExchangeItems({Price}, {Goods}));
	TestEqual(TEXT("Currency charged once"), Inventory->CountItemsOfExactClass(UCurrency_ItemObject::StaticClass()), int64(15));
	Goods.ItemClass = UFood_ItemObject::StaticClass();
	TestFalse(TEXT("Full inventory rejects purchase"), Inventory->TryExchangeItems({Price}, {Goods}));
	TestEqual(TEXT("Rejected purchase preserves money"), Inventory->CountItemsOfExactClass(UCurrency_ItemObject::StaticClass()), int64(15));
	Price.Quantity = 15;
	TestTrue(TEXT("Payment freeing a slot permits output"), Inventory->TryExchangeItems({Price}, {Goods}));
	TestEqual(TEXT("Payment exhausted correctly"), Inventory->CountItemsOfExactClass(UCurrency_ItemObject::StaticClass()), int64(0));
	UItemBlueprint* Recipe = NewObject<UItemBlueprint>(Character->CraftingComp);
	Recipe->RequiredItems.Add(UFood_ItemObject::StaticClass());
	Recipe->CraftedItem = UPotion_ItemObject::StaticClass();
	Recipe->CraftedQuantity = 1;
	Character->CraftingComp->Recipes.Add(Recipe);
	TestTrue(TEXT("Crafting consumes ingredient and creates output"), Character->CraftingComp->TryCraftRecipe(0));
	TestEqual(TEXT("Ingredient consumed"), Inventory->CountItemsOfExactClass(UFood_ItemObject::StaticClass()), int64(0));
	TestEqual(TEXT("One crafted output"), Inventory->CountItemsOfExactClass(UPotion_ItemObject::StaticClass()), int64(1));
	TestFalse(TEXT("Repeated craft cannot create free output"), Character->CraftingComp->TryCraftRecipe(0));
	TestFalse(TEXT("Unlisted recipe rejected"), Character->CraftingComp->TryCraftRecipe(100));
	Price.Quantity = -1;
	TestFalse(TEXT("Negative transaction rejected"), Inventory->TryExchangeItems({Price}, {}));
	Inventory->Capacity = 5;
	Inventory->TryAddItemFromClass(UCurrency_ItemObject::StaticClass(), 10);
	FRPGVendorOffer Offer;
	Offer.OfferId = FName("FoodForCoins"); Offer.ItemClass = UFood_ItemObject::StaticClass(); Offer.RemainingStock = 2;
	FRPGInventoryAmount VendorPrice; VendorPrice.ItemClass = UCurrency_ItemObject::StaticClass(); VendorPrice.Quantity = 3;
	Offer.Price.Add(VendorPrice);
	ARPGVendor* Vendor = Context.World->SpawnActor<ARPGVendor>(Character->GetActorLocation(), FRotator::ZeroRotator);
	Vendor->Offers.Add(Offer);
	TestTrue(TEXT("Vendor purchase commits price, output and stock atomically"), Vendor->TryPurchase(Character, Offer.OfferId, 2));
	TestEqual(TEXT("Finite vendor stock decrements exactly"), Vendor->Offers[0].RemainingStock, 0);
	TestEqual(TEXT("Vendor charges the multiplied price"), Inventory->CountItemsOfExactClass(UCurrency_ItemObject::StaticClass()), int64(4));
	TestFalse(TEXT("Exhausted vendor stock rejects replay"), Vendor->TryPurchase(Character, Offer.OfferId, 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGRandomRewardTest, "DuTraveller.Restoration.RandomRewardPools",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGRandomRewardTest::RunTest(const FString& Parameters)
{
	URPGRandomQuestDefinition* Definition = NewObject<URPGRandomQuestDefinition>();
	Definition->QuestId = FName("RewardTest");
	FRPGDynamicObjective Objective; Objective.ObjectiveId = FName("Inspect"); Objective.Event = FName("Interacted");
	Definition->Objectives.Add(Objective);
	FRPGRandomRewardEntry A; A.ItemClass = UCurrency_ItemObject::StaticClass(); A.MinimumQuantity = 2; A.MaximumQuantity = 5;
	FRPGRandomRewardEntry B; B.ItemClass = UFood_ItemObject::StaticClass(); B.Weight = 3;
	Definition->RandomRewards = {A, B}; Definition->RandomRewardRolls = 2; Definition->bRewardsWithoutReplacement = true;
	FRandomStream First(91), Second(91);
	TArray<FRPGInventoryAmount> FirstRoll, SecondRoll;
	TestTrue(TEXT("Valid weighted pool rolls"), Definition->RollRewards(First, FirstRoll));
	TestTrue(TEXT("Same seed can replay the selection"), Definition->RollRewards(Second, SecondRoll));
	if (FirstRoll.Num() == 2 && SecondRoll.Num() == 2)
	{
		TestTrue(TEXT("Without replacement returns distinct rewards"), FirstRoll[0].ItemClass != FirstRoll[1].ItemClass);
		for (int32 Index = 0; Index < 2; ++Index)
		{
			TestTrue(TEXT("Deterministic class selection"), FirstRoll[Index].ItemClass == SecondRoll[Index].ItemClass);
			TestEqual(TEXT("Deterministic quantities"), FirstRoll[Index].Quantity, SecondRoll[Index].Quantity);
			TestTrue(TEXT("Quantity stays inside configured range"), FirstRoll[Index].Quantity >= 1 && FirstRoll[Index].Quantity <= 5);
		}
	}
	else AddError(TEXT("Expected two rewards"));
	Definition->RandomRewards[0].Weight = 0;
	TestFalse(TEXT("Invalid zero weights rejected"), Definition->RollRewards(First, FirstRoll));
	Definition->RandomRewards[0].Weight = 1;
	Definition->RandomRewardRolls = 3;
	TestFalse(TEXT("Impossible distinct draws rejected"), Definition->IsConfigured());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGSpawnAreaTest, "DuTraveller.Restoration.RandomSpawnAreas",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGSpawnAreaTest::RunTest(const FString& Parameters)
{
	FRestorationWorld Context;
	ARPGSpawnArea* Area = Context.World->SpawnActor<ARPGSpawnArea>();
	Area->AreaId = FName("TestArea"); Area->LocationId = FName("Forest"); Area->bRequireGround = false; Area->SpawnInterval = 0;
	Area->MaximumAlive = 2;
	FRPGWeightedSpawn Entry; Entry.ActorClass = ARPGPickupItem::StaticClass(); Area->SpawnPool.Add(Entry);
	const FGuid QuestId = FGuid::NewGuid();
	AActor* First = Area->TrySpawnOne(QuestId);
	AActor* Second = Area->TrySpawnOne(QuestId);
	TestNotNull(TEXT("First random actor spawned"), First);
	TestNotNull(TEXT("Second random actor spawned"), Second);
	TestNull(TEXT("Population limit enforced"), Area->TrySpawnOne(QuestId));
	if (First)
	{
		TestTrue(TEXT("Spawn remains inside area"), Area->ContainsPosition(First->GetActorLocation()));
		const auto* Link = First->FindComponentByClass<URPGSpawnContextComponent>();
		TestTrue(TEXT("Spawn linked to quest and location"), Link && Link->QuestInstanceId == QuestId && Link->LocationId == FName("Forest") && Link->SpawnId.IsValid());
		First->Destroy();
		TestEqual(TEXT("Destroyed actor releases population slot"), Area->AliveActors.Num(), 1);
	}
	TestNotNull(TEXT("Replacement may spawn within remaining budget"), Area->TrySpawnOne(QuestId));
	FRPGSpawnAreaSnapshot Snapshot = Area->CaptureSpawnState();
	Area->ClearSpawnedActors();
	TestEqual(TEXT("Explicit cleanup empties tracking"), Area->AliveActors.Num(), 0);
	ARPGSpawnArea* Restored = Context.World->SpawnActor<ARPGSpawnArea>();
	Restored->AreaId = Area->AreaId; Restored->LocationId = Area->LocationId; Restored->bRequireGround = false;
	Restored->MaximumAlive = 2; Restored->SpawnPool = Area->SpawnPool;
	TestTrue(TEXT("Fresh area restores saved population"), Restored->RestoreSpawnState(Snapshot));
	TestEqual(TEXT("Restored population matches snapshot"), Restored->AliveActors.Num(), Snapshot.Actors.Num());
	TestFalse(TEXT("Repeated restore cannot duplicate actors"), Restored->RestoreSpawnState(Snapshot));
	ARPGSpawnArea* GroundArea = Context.World->SpawnActor<ARPGSpawnArea>();
	GroundArea->AreaId = FName("NeedsGround"); GroundArea->LocationId = FName("Forest"); GroundArea->SpawnPool = Area->SpawnPool;
	TestNull(TEXT("Missing ground fails placement"), GroundArea->TrySpawnOne(FGuid()));
	TestTrue(TEXT("Failed placement does not add population"), GroundArea->AliveActors.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGDynamicQuestTest, "DuTraveller.Restoration.LocationQuestsAndRewardClaims",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGDynamicQuestTest::RunTest(const FString& Parameters)
{
	FRestorationWorld Context;
	ARPGCharacter* Pawn = Context.Character();
	ARPGPlayerState* PS = Context.World->SpawnActor<ARPGPlayerState>(); Pawn->SetPlayerState(PS); Pawn->Initialize(nullptr);
	ARPGRandomQuestSpawner* Spawner = Context.World->SpawnActor<ARPGRandomQuestSpawner>();
	Spawner->AreaId = FName("ForestOffers"); Spawner->LocationId = FName("Forest"); Spawner->bRequireGround = false; Spawner->SpawnInterval = 0;
	FRPGWeightedSpawn ActorEntry; ActorEntry.ActorClass = ARPGPickupItem::StaticClass(); Spawner->SpawnPool.Add(ActorEntry);
	URPGRandomQuestDefinition* Definition = NewObject<URPGRandomQuestDefinition>(Spawner);
	Definition->QuestId = FName("ForestInspection"); Definition->AllowedLocations.Add(FName("Forest"));
	FRPGDynamicObjective Objective; Objective.ObjectiveId = FName("Inspect"); Objective.Event = FName("Interacted"); Objective.RequiredCount = 2;
	Definition->Objectives.Add(Objective);
	FRPGInventoryAmount Fixed; Fixed.ItemClass = UCurrency_ItemObject::StaticClass(); Fixed.Quantity = 10; Definition->GuaranteedRewards.Add(Fixed);
	FRPGRandomRewardEntry Random; Random.ItemClass = UFood_ItemObject::StaticClass(); Random.MinimumQuantity = 1; Random.MaximumQuantity = 3;
	Definition->RandomRewards.Add(Random); Definition->ExperienceReward = 25.f;
	URPGQuestGroupDefinition* Group = NewObject<URPGQuestGroupDefinition>(Spawner); Group->GroupId = FName("ForestGroup");
	FRPGWeightedQuest Choice; Choice.Definition = Definition; Group->Quests.Add(Choice); Spawner->QuestGroups.Add(Group);
	TestTrue(TEXT("Location creates a quest offer"), Spawner->GenerateOffer());
	TestFalse(TEXT("Offer pool does not duplicate a quest"), Spawner->GenerateOffer());
	if (Spawner->Offers.IsEmpty()) return false;
	const FGuid OfferId = Spawner->Offers[0].OfferId;
	Pawn->SetActorLocation(FVector(10000, 0, 0));
	TestFalse(TEXT("Out-of-location acceptance rejected"), PS->DynamicQuests->TryAcceptOffer(Spawner, OfferId));
	Pawn->SetActorLocation(FVector::ZeroVector);
	Definition->MinimumLevel = 2;
	TestFalse(TEXT("Level eligibility enforced"), PS->DynamicQuests->TryAcceptOffer(Spawner, OfferId));
	Definition->MinimumLevel = 1;
	Definition->PrerequisiteQuests.Add(FName("MissingChainStep"));
	TestFalse(TEXT("Quest-chain prerequisite enforced"), PS->DynamicQuests->TryAcceptOffer(Spawner, OfferId));
	Definition->PrerequisiteQuests.Reset();
	TestTrue(TEXT("Eligible player accepts offer"), PS->DynamicQuests->TryAcceptOffer(Spawner, OfferId));
	TestFalse(TEXT("Repeated accept cannot reroll"), PS->DynamicQuests->TryAcceptOffer(Spawner, OfferId));
	if (PS->DynamicQuests->Records.IsEmpty()) return false;
	const FGuid InstanceId = PS->DynamicQuests->Records[0].InstanceId;
	URPGRandomQuestDefinition* Followup = NewObject<URPGRandomQuestDefinition>(Spawner);
	Followup->QuestId = FName("Followup"); Followup->Objectives = Definition->Objectives; Followup->RandomRewardRolls = 0;
	FRPGWeightedQuest FollowupChoice; FollowupChoice.Definition = Followup; Group->Quests.Add(FollowupChoice);
	TestTrue(TEXT("Quest group offers another definition"), Spawner->GenerateOffer());
	const FGuid FollowupOffer = Spawner->Offers.Last().OfferId;
	TestFalse(TEXT("Group active limit enforced"), PS->DynamicQuests->TryAcceptOffer(Spawner, FollowupOffer));
	Followup->PrerequisiteQuests.Add(Definition->QuestId);
	const int32 RolledQuantity = PS->DynamicQuests->Records[0].RolledRewards.Last().Quantity;
	TestFalse(TEXT("Unfinished quest cannot claim"), PS->DynamicQuests->TryClaimReward(InstanceId));
	AActor* Target = Spawner->TrySpawnOne(InstanceId);
	if (!TestNotNull(TEXT("Linked objective actor spawns"), Target)) return false;
	auto* Link = Target->FindComponentByClass<URPGSpawnContextComponent>();
	TestTrue(TEXT("Correct objective event advances quest"), Link->ReportQuestEvent(FName("Interacted")));
	TestFalse(TEXT("Repeated actor event cannot duplicate credit"), Link->ReportQuestEvent(FName("Interacted")));
	AActor* Other = Spawner->TrySpawnOne(FGuid::NewGuid());
	if (!TestNotNull(TEXT("Unrelated actor spawns"), Other)) return false;
	TestFalse(TEXT("Wrong quest actor cannot give credit"), PS->DynamicQuests->RecordObjectiveEvent(FName("Interacted"), Other));
	Other->FindComponentByClass<URPGSpawnContextComponent>()->QuestInstanceId = InstanceId;
	TestTrue(TEXT("Second linked actor completes objective"), PS->DynamicQuests->RecordObjectiveEvent(FName("Interacted"), Other));
	Pawn->InventoryComp->Capacity = 1; Pawn->InventoryComp->TryAddItemFromClass(UGear_ItemObject::StaticClass(), 1);
	TestFalse(TEXT("Full inventory blocks complete reward transaction"), PS->DynamicQuests->TryClaimReward(InstanceId));
	TestEqual(TEXT("Failed claim grants no currency"), Pawn->InventoryComp->CountItemsOfExactClass(UCurrency_ItemObject::StaticClass()), int64(0));
	TestEqual(TEXT("Failed claim grants no experience"), PS->EXP, 0.f);
	TestEqual(TEXT("Failed claim preserves reward roll"), PS->DynamicQuests->Records[0].RolledRewards.Last().Quantity, RolledQuantity);
	Pawn->InventoryComp->Capacity = 8;
	TestTrue(TEXT("Capacity retry grants original rewards"), PS->DynamicQuests->TryClaimReward(InstanceId));
	TestEqual(TEXT("Currency reward granted exactly once"), Pawn->InventoryComp->CountItemsOfExactClass(UCurrency_ItemObject::StaticClass()), int64(10));
	TestEqual(TEXT("Selected random quantity delivered"), Pawn->InventoryComp->CountItemsOfExactClass(UFood_ItemObject::StaticClass()), int64(RolledQuantity));
	TestEqual(TEXT("Experience granted once"), PS->EXP, 25.f);
	TestFalse(TEXT("Repeated claim rejected"), PS->DynamicQuests->TryClaimReward(InstanceId));
	TestTrue(TEXT("Completion supports quest-chain prerequisites"), PS->DynamicQuests->HasCompletedQuest(Definition->QuestId));
	TestTrue(TEXT("Completed prerequisite unlocks followup in same group"), PS->DynamicQuests->TryAcceptOffer(Spawner, FollowupOffer));
	URPGSpawnQuestSaveGame* Save = NewObject<URPGSpawnQuestSaveGame>(); Save->PlayerQuests = PS->DynamicQuests->CaptureQuestState();
	Save->SpawnAreas.Add(Spawner->CaptureSpawnState());
	TArray<uint8> Bytes;
	TestTrue(TEXT("Snapshot serializes to real save-game archive"), UGameplayStatics::SaveGameToMemory(Save, Bytes));
	URPGSpawnQuestSaveGame* Loaded = Cast<URPGSpawnQuestSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (!TestNotNull(TEXT("Save-game archive reloads"), Loaded)) return false;
	ARPGPlayerState* NewPS = Context.World->SpawnActor<ARPGPlayerState>();
	TestTrue(TEXT("Fresh player restores quest snapshot"), NewPS->DynamicQuests->RestoreQuestState(Loaded->PlayerQuests));
	TestTrue(TEXT("Claim ledger survives load"), NewPS->DynamicQuests->HasCompletedQuest(Definition->QuestId));
	ARPGCharacter* NewPawn = Context.Character(); NewPawn->SetPlayerState(NewPS); NewPawn->Initialize(nullptr);
	TestFalse(TEXT("Loaded claimed quest cannot pay again"), NewPS->DynamicQuests->TryClaimReward(InstanceId));
	TestFalse(TEXT("Live quest state cannot be overwritten with old save"), PS->DynamicQuests->RestoreQuestState(Loaded->PlayerQuests));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGSpawnCollisionTest, "DuTraveller.Restoration.SpawnGroundAndCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGSpawnCollisionTest::RunTest(const FString& Parameters)
{
	FRestorationWorld Context(true);
	AStaticMeshActor* Floor = Context.World->SpawnActor<AStaticMeshActor>();
	Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Floor->SetActorScale3D(FVector(20, 20, 0.1));
	Floor->GetStaticMeshComponent()->SetCollisionProfileName(FName("BlockAll"));
	Context.World->Tick(LEVELTICK_All, 0.02f);
	ARPGSpawnArea* Area = Context.World->SpawnActor<ARPGSpawnArea>();
	Area->AreaId = FName("GroundTest"); Area->LocationId = FName("Forest"); Area->SpawnInterval = 0;
	FRPGWeightedSpawn Entry; Entry.ActorClass = ARPGPickupItem::StaticClass(); Area->SpawnPool.Add(Entry);
	AActor* OnGround = Area->TrySpawnOne(FGuid());
	TestNotNull(TEXT("Ground trace finds valid supported placement"), OnGround);
	if (OnGround) TestTrue(TEXT("Actor clears floor by configured half height"), OnGround->GetActorLocation().Z >= Area->PlacementHalfExtent.Z);
	Area->ClearSpawnedActors();
	Area->bRequireGround = false;
	Floor->SetActorScale3D(FVector(20, 20, 20));
	Context.World->Tick(LEVELTICK_All, 0.02f);
	TestNull(TEXT("Solid obstruction rejects every candidate"), Area->TrySpawnOne(FGuid()));
	TestTrue(TEXT("Obstructed area retains zero population"), Area->AliveActors.IsEmpty());
	Floor->SetActorScale3D(FVector(20, 20, 0.1));
	Floor->GetStaticMeshComponent()->SetCollisionObjectType(ECC_Pawn);
	Context.World->Tick(LEVELTICK_All, 0.02f);
	ARPGSpawnArea* PawnSurface = Context.World->SpawnActor<ARPGSpawnArea>();
	PawnSurface->AreaId = FName("PawnSurface"); PawnSurface->LocationId = FName("Forest"); PawnSurface->SpawnPool.Add(Entry);
	TestNull(TEXT("Pawn collision cannot masquerade as ground"), PawnSurface->TrySpawnOne(FGuid()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGItemPresentationTest, "DuTraveller.Restoration.ItemPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGItemPresentationTest::RunTest(const FString& Parameters)
{
	UGear_ItemObject* Gear = NewObject<UGear_ItemObject>();
	Gear->ItemDisplayName = FText::FromString(TEXT("Bronze Sword"));
	Gear->Slot = EEquipableSlot::EIS_PrimaryWeapon;
	Gear->AttributeBonuses.Add(FName("Strength"), 5.f);
	TestEqual(TEXT("Gear hierarchy label"), Gear->GetItemTypeText().ToString(), FString(TEXT("Gear")));
	TestTrue(TEXT("Gear tooltip contains slot"), Gear->GetTooltipDetailsText().ToString().Contains(TEXT("Primary Weapon")));
	TestTrue(TEXT("Gear tooltip contains stat bonus"), Gear->GetTooltipDetailsText().ToString().Contains(TEXT("Strength")));

	UPotion_ItemObject* Potion = NewObject<UPotion_ItemObject>();
	Potion->SetByCallerMagnitudes.Add(FName("Health"), 25.f);
	TestEqual(TEXT("Potion hierarchy label"), Potion->GetItemTypeText().ToString(), FString(TEXT("Potion")));
	TestTrue(TEXT("Consumable tooltip contains effect"), Potion->GetTooltipDetailsText().ToString().Contains(TEXT("Health")));

	UItemBlueprint* Recipe = NewObject<UItemBlueprint>();
	Recipe->CraftedItem = UPotion_ItemObject::StaticClass();
	FRPGInventoryAmount Ingredient;
	Ingredient.ItemClass = UFood_ItemObject::StaticClass();
	Ingredient.Quantity = 2;
	Recipe->Ingredients.Add(Ingredient);
	const FString RecipeDetails = Recipe->GetTooltipDetailsText().ToString();
	TestTrue(TEXT("Recipe tooltip contains output"), RecipeDetails.Contains(TEXT("Crafts:")));
	TestTrue(TEXT("Recipe tooltip contains ingredient quantity"), RecipeDetails.Contains(TEXT("x2")));

	URPG_ItemToolTip* Tooltip = NewObject<URPG_ItemToolTip>();
	Tooltip->SetItem(Gear);
	TestEqual(TEXT("Tooltip receives item name"), Tooltip->ItemDisplayName.ToString(), FString(TEXT("Bronze Sword")));
	TestTrue(TEXT("Tooltip receives hierarchy details"), Tooltip->DetailsText.ToString().Contains(TEXT("Strength")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGPartyAuthorityTest, "DuTraveller.Restoration.PartyAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGPartyAuthorityTest::RunTest(const FString& Parameters)
{
	FRestorationWorld Context;
	ARPGPlayerState* Leader = Context.World->SpawnActor<ARPGPlayerState>();
	ARPGPlayerState* Member = Context.World->SpawnActor<ARPGPlayerState>();
	ARPGPlayerState* Third = Context.World->SpawnActor<ARPGPlayerState>();
	TestEqual(TEXT("New party leaders default to four-player mode"), Leader->Party->PartySizeLimit, 4);
	Leader->Party->RequestSetPartySizeLimit(10);
	TestEqual(TEXT("Leader can select ten-player mode before inviting"), Leader->Party->PartySizeLimit, 10);
	Leader->Party->RequestInvite(Member);
	TestTrue(TEXT("Invite creates a stable party identifier"), Leader->Party->PartyId.IsValid());
	TestEqual(TEXT("Invite is owner-targeted"), Member->Party->PendingInviter.Get(), Leader);
	Member->Party->AcceptInvite();
	TestEqual(TEXT("Accepted members share party ID"), Member->Party->PartyId, Leader->Party->PartyId);
	TestEqual(TEXT("Both rosters contain two members"), Leader->Party->Members.Num(), 2);
	TestEqual(TEXT("Party size mode synchronizes to joining member"), Member->Party->PartySizeLimit, 10);
	Member->Party->RequestSetPartySizeLimit(20);
	TestEqual(TEXT("Non-leader cannot change party size"), Leader->Party->PartySizeLimit, 10);
	Leader->Party->RequestSetPartySizeLimit(20);
	TestEqual(TEXT("Leader can select twenty-player mode"), Member->Party->PartySizeLimit, 20);
	Member->Party->RequestInvite(Third);
	TestFalse(TEXT("Non-leader cannot invite"), Third->Party->PendingPartyId.IsValid());
	Leader->Party->RequestInvite(Third); Third->Party->AcceptInvite();
	TestEqual(TEXT("Leader can add a third member"), Third->Party->Members.Num(), 3);
	Leader->Party->RequestSetPartySizeLimit(4);
	TestEqual(TEXT("Leader can lower capacity when roster fits"), Third->Party->PartySizeLimit, 4);
	const FGuid PartyId = Leader->Party->PartyId;
	Leader->Party->LeaveParty();
	TestFalse(TEXT("Leaver clears local party state"), Leader->Party->PartyId.IsValid());
	TestEqual(TEXT("Leadership transfers when leader leaves"), Member->Party->Leader.Get(), Member);
	TestEqual(TEXT("Remaining party keeps identity"), Member->Party->PartyId, PartyId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGAICombatLoopTest, "DuTraveller.Restoration.AICombatLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGAICombatLoopTest::RunTest(const FString& Parameters)
{
	FRestorationWorld Context;
	ARPGAICharacter* Enemy = Context.World->SpawnActor<ARPGAICharacter>();
	ARPGAIController* Controller = Context.World->SpawnActor<ARPGAIController>();
	ARPGCharacter* Player = Context.Character();
	Enemy->Initialize(nullptr); Controller->Possess(Enemy);
	Enemy->SetActorLocation(FVector::ZeroVector); Player->SetActorLocation(FVector(100.f, 0.f, 0.f));
	Player->FactionId = FName("Player");
	Enemy->GetAbilitySystemComponent()->SetNumericAttributeBase(URPGAttributeSet::GetAttackPowerAttribute(), 10.f);
	Enemy->GetAbilitySystemComponent()->SetNumericAttributeBase(URPGAttributeSet::GetStaminaAttribute(), 100.f);
	Player->GetAbilitySystemComponent()->SetNumericAttributeBase(URPGAttributeSet::GetHealthAttribute(), 100.f);
	Player->GetAbilitySystemComponent()->SetNumericAttributeBase(URPGAttributeSet::GetMaxHealthAttribute(), 100.f);
	URPGCombatAbilityDefinition* Attack = NewObject<URPGCombatAbilityDefinition>(Controller);
	Attack->AbilityId = FName("AI.BasicAttack"); Attack->DisplayName = FText::FromString(TEXT("Basic Attack"));
	Attack->BaseDamageByRank = {10.f}; Attack->AttackPowerScale = 1.f; Attack->Range = 250.f; Attack->Cooldown = 0.f;
	Controller->AttackAbility = Attack;
	TestEqual(TEXT("AI selects hostile living character"), Controller->AcquireCombatTarget(), Player);
	TestTrue(TEXT("AI activates through master action component"), Controller->RunCombatDecision());
	TestEqual(TEXT("Server damage reaches GAS health"), Player->GetRPGAttributes()->GetHealth(), 80.f);
	TestEqual(TEXT("Legacy/quest health bridge follows GAS"), Player->AttributeComp->Health, 80.f);
	Player->ReceiveCombatDamage(1000.f, Enemy, FName("AI.BasicAttack"));
	TestFalse(TEXT("AI refuses dead target"), Controller->RunCombatDecision());
	TestNull(TEXT("Dead target is removed from acquisition"), Controller->AcquireCombatTarget());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGPartyQuestTest, "DuTraveller.Restoration.PartyQuestSharing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGPartyQuestTest::RunTest(const FString& Parameters)
{
	FRestorationWorld Context;
	ARPGPlayerState* Leader = Context.World->SpawnActor<ARPGPlayerState>();
	ARPGPlayerState* Member = Context.World->SpawnActor<ARPGPlayerState>();
	ARPGCharacter* LeaderPawn = Context.Character(); LeaderPawn->SetPlayerState(Leader); LeaderPawn->Initialize(nullptr);
	ARPGCharacter* MemberPawn = Context.Character(); MemberPawn->SetPlayerState(Member); MemberPawn->Initialize(nullptr);
	Leader->Party->RequestInvite(Member); Member->Party->AcceptInvite();
	ARPGRandomQuestSpawner* Spawner = Context.World->SpawnActor<ARPGRandomQuestSpawner>();
	Spawner->AreaId = FName("PartyArea"); Spawner->LocationId = FName("PartyLocation"); Spawner->bRequireGround = false; Spawner->SpawnInterval = 0;
	URPGRandomQuestDefinition* Definition = NewObject<URPGRandomQuestDefinition>(Spawner);
	Definition->QuestId = FName("PartyHunt"); Definition->Audience = ERPGQuestAudience::Party; Definition->RandomRewardRolls = 0;
	FRPGDynamicObjective Objective; Objective.ObjectiveId = FName("Defeat"); Objective.Event = FName("Defeated"); Objective.bRequireQuestActor = false;
	Definition->Objectives.Add(Objective);
	FRPGInventoryAmount Reward; Reward.ItemClass = UCurrency_ItemObject::StaticClass(); Reward.Quantity = 5; Definition->GuaranteedRewards.Add(Reward);
	Definition->ExperienceReward = 25.f;
	URPGQuestGroupDefinition* Group = NewObject<URPGQuestGroupDefinition>(Spawner); Group->GroupId = FName("PartyGroup");
	FRPGWeightedQuest Choice; Choice.Definition = Definition; Group->Quests.Add(Choice); Spawner->QuestGroups.Add(Group);
	TestTrue(TEXT("Party quest offer generated"), Spawner->GenerateOffer());
	TestTrue(TEXT("Leader accepts for current party"), Leader->DynamicQuests->TryAcceptOffer(Spawner, Spawner->Offers[0].OfferId));
	TestEqual(TEXT("Party member receives shared record"), Member->DynamicQuests->Records.Num(), 1);
	TestEqual(TEXT("Shared records use one instance"), Member->DynamicQuests->Records[0].InstanceId, Leader->DynamicQuests->Records[0].InstanceId);
	TestTrue(TEXT("One member event advances party"), Leader->DynamicQuests->RecordObjectiveEvent(FName("Defeated"), nullptr));
	TestEqual(TEXT("Leader quest completes"), Leader->DynamicQuests->Records[0].Status, ERPGDynamicQuestStatus::Complete);
	TestEqual(TEXT("Member quest completes"), Member->DynamicQuests->Records[0].Status, ERPGDynamicQuestStatus::Complete);
	TestTrue(TEXT("Party XP grant succeeds"), Leader->GrantExperience(12.f));
	TestEqual(TEXT("XP earner receives full amount"), Leader->EXP, 12.f);
	TestEqual(TEXT("Party member receives same full amount"), Member->EXP, 12.f);
	const FGuid Instance = Leader->DynamicQuests->Records[0].InstanceId;
	TestTrue(TEXT("Leader claims own reward once"), Leader->DynamicQuests->TryClaimReward(Instance));
	TestTrue(TEXT("Member claims own reward once"), Member->DynamicQuests->TryClaimReward(Instance));
	TestEqual(TEXT("Leader receives exact reward"), LeaderPawn->InventoryComp->CountItemsOfExactClass(UCurrency_ItemObject::StaticClass()), int64(5));
	TestEqual(TEXT("Member receives exact reward"), MemberPawn->InventoryComp->CountItemsOfExactClass(UCurrency_ItemObject::StaticClass()), int64(5));
	TestEqual(TEXT("Each member receives own quest XP once"), Member->EXP, 37.f);
	TestFalse(TEXT("Party reward replay rejected"), Member->DynamicQuests->TryClaimReward(Instance));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGGuildSettlementTest, "DuTraveller.Restoration.GuildSettlementEconomy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGGuildSettlementTest::RunTest(const FString& Parameters)
{
	FRestorationWorld Context;
	ARPGPlayerState* Leader=Context.World->SpawnActor<ARPGPlayerState>(); ARPGCharacter* LeaderPawn=Context.Character(); LeaderPawn->SetPlayerState(Leader); LeaderPawn->Initialize(nullptr);
	ARPGPlayerState* Member=Context.World->SpawnActor<ARPGPlayerState>(); ARPGCharacter* MemberPawn=Context.Character(); MemberPawn->SetPlayerState(Member); MemberPawn->Initialize(nullptr);
	ARPGGuildState* Guild=Context.World->SpawnActor<ARPGGuildState>();
	TestTrue(TEXT("Leader creates stable guild and settlement"), Guild->InitializeGuild(FName("TestGuild"),Leader));
	TestTrue(TEXT("Leader adds member"), Guild->AddMember(Leader,Member));
	LeaderPawn->InventoryComp->TryAddItemFromClass(UFood_ItemObject::StaticClass(),7);
	TestTrue(TEXT("Member-authorized deposit is atomic"), Guild->Deposit(Leader,UFood_ItemObject::StaticClass(),6));
	TestEqual(TEXT("Guild bank receives deposited materials"),Guild->CountBankItem(UFood_ItemObject::StaticClass()),6);
	TestFalse(TEXT("Member without withdraw permission is rejected"),Guild->Withdraw(Member,UFood_ItemObject::StaticClass(),1));
	TestTrue(TEXT("Leader withdraws from bank"),Guild->Withdraw(Leader,UFood_ItemObject::StaticClass(),2));
	URPGBuildingDefinition* Definition=NewObject<URPGBuildingDefinition>(Guild); Definition->DefinitionId=FName("Farm"); Definition->BuildingClass=ARPGFarmBuilding::StaticClass(); Definition->InteractionRange=1000.f;
	FRPGInventoryAmount Cost; Cost.ItemClass=UFood_ItemObject::StaticClass(); Cost.Quantity=4; Definition->ConstructionCosts.Add(Cost);
	ARPGBuilding* Built=Guild->PlaceBuilding(Leader,Definition,FTransform(FRotator::ZeroRotator,FVector(100,0,0)));
	if(!TestNotNull(TEXT("Material-funded building spawns"),Built)) return false;
	TestEqual(TEXT("Construction consumes exact bank cost"),Guild->CountBankItem(UFood_ItemObject::StaticClass()),0);
	TestNull(TEXT("Repeated construction cannot duplicate without materials"),Guild->PlaceBuilding(Leader,Definition,FTransform(FRotator::ZeroRotator,FVector(300,0,0))));
	ARPGFarmBuilding* Farm=CastChecked<ARPGFarmBuilding>(Built); Farm->SeedItemClass=UFood_ItemObject::StaticClass(); Farm->HarvestItemClass=UPotion_ItemObject::StaticClass(); Farm->HarvestQuantity=2;
	TestTrue(TEXT("Authorized member plants one owned seed"),Farm->Plant(Leader));
	Farm->FarmState=ERPGFarmState::Ready;
	TestTrue(TEXT("Ready farm harvest adds configured output"),Farm->Harvest(Leader));
	TestEqual(TEXT("Harvest quantity is exact"),LeaderPawn->InventoryComp->CountItemsOfExactClass(UPotion_ItemObject::StaticClass()),int64(2));
	TestFalse(TEXT("Repeated harvest cannot duplicate output"),Farm->Harvest(Leader));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGHotbarInputSwapTest, "DuTraveller.Restoration.HotbarInputAndSwap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGHotbarInputSwapTest::RunTest(const FString& Parameters)
{
	FRestorationWorld Context;
	ARPGPlayerController* PC = Context.World->SpawnActor<ARPGPlayerController>();
	ARPGPlayerState* PS = Context.World->SpawnActor<ARPGPlayerState>();
	ARPGCharacter* Pawn = Context.Character();
	PC->PlayerState = PS; PC->Possess(Pawn); Pawn->SetPlayerState(PS); Pawn->Initialize(nullptr);
	PC->SetPlayer(NewObject<ULocalPlayer>(GEngine));
	PC->InitInputSystem();
	URPGCombatAbilityDefinition* A = NewObject<URPGCombatAbilityDefinition>(PS->Skills);
	A->AbilityId = TEXT("Abilities.Melee.Slash"); A->bRequiresTarget = false; A->Cooldown = 0; A->StaminaCost = 1;
	A->ActionClass = URPGAction::StaticClass();
	URPGCombatAbilityDefinition* B = NewObject<URPGCombatAbilityDefinition>(PS->Skills);
	B->AbilityId = TEXT("Abilities.Fire.FireBolt"); B->bRequiresTarget = false;
	PS->Skills->AvailableAbilities = {A, B}; PS->SpellPoints = 3;
	TestTrue(TEXT("Learn A"), PS->Skills->TryLearnAbility(A));
	TestTrue(TEXT("Learn B"), PS->Skills->TryLearnAbility(B));
	PS->Skills->TryEquipHotbar(A, 0); PS->Skills->TryEquipHotbar(B, 1);
	TestTrue(TEXT("Move swaps occupied slots"), PS->Skills->TryEquipHotbar(A, 1));
	TestEqual(TEXT("No duplicate entries"), PS->Skills->Hotbar.Num(), 2);
	const FRPGHotbarSlot* Displaced = PS->Skills->Hotbar.FindByPredicate([](const FRPGHotbarSlot& S) { return S.Slot == 0; });
	TestTrue(TEXT("Displaced skill moved back"), Displaced && Displaced->Definition == B);
	PS->Skills->TryEquipHotbar(A, 2);
	TestTrue(TEXT("Vacated slot sends clear event"), PS->Skills->GetAbilityPresentation().ContainsByPredicate([](const FRPGAbilityInfo& I) {
		return I.InputTag.ToString() == TEXT("InputTag.RMB") && I.AbilityTag.ToString() == TEXT("Abilities.None"); }));
	PS->AbilitySystem->SetNumericAttributeBase(URPGAttributeSet::GetStaminaAttribute(), 20);
	FInputKeyEventArgs Key; Key.Key = EKeys::One; Key.Event = IE_Pressed;
	PC->InputKey(Key);
	TestEqual(TEXT("Viewport key executes equipped action"), Pawn->ActionComp->CommittedAction.AbilityId, A->AbilityId);
	TestEqual(TEXT("Input charges once"), PS->Attributes->GetStamina(), 19.f);
	PC->ActivePanel = TEXT("Inventory"); PC->InputKey(Key);
	TestEqual(TEXT("Panel blocks combat keys"), PS->Attributes->GetStamina(), 19.f);
	PC->ActivePanel = NAME_None; Key.Event = IE_Released; PC->InputKey(Key);
	TestEqual(TEXT("Release does not fire again"), PS->Attributes->GetStamina(), 19.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGSpellReleasePipelineTest, "DuTraveller.Restoration.SpellReleasePipeline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGSpellReleasePipelineTest::RunTest(const FString& Parameters)
{
	FRestorationWorld Context;
	ARPGPlayerState* PS = Context.World->SpawnActor<ARPGPlayerState>();
	ARPGCharacter* Caster = Context.Character();
	Caster->SetPlayerState(PS); Caster->Initialize(nullptr);
	PS->AbilitySystem->SetNumericAttributeBase(URPGAttributeSet::GetManaAttribute(), 50.f);

	UAnimMontage* CastMontage = LoadObject<UAnimMontage>(nullptr,
		TEXT("/Game/DTIntegration/Animation/Combat/DT_Standing_1H_Cast_Spell_01_UEFN.DT_Standing_1H_Cast_Spell_01_UEFN"));
	if (!TestNotNull(TEXT("UEFN cast montage loads"), CastMontage)) return true;

	URPGCombatAbilityDefinition* Fire = NewObject<URPGCombatAbilityDefinition>(PS->Skills);
	Fire->AbilityId = TEXT("Abilities.Fire.FireBolt");
	Fire->Kind = ERPGAbilityKind::Spell;
	Fire->bRequiresTarget = false;
	Fire->ManaCost = 5.f;
	Fire->Cooldown = 0.f;
	Fire->BaseDamageByRank = {25.f};
	Fire->ActionClass = URPGAction::StaticClass();
	Fire->ReleaseEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Animation.SpellRelease"));
	Fire->ProjectileClass = ARPGProjectile::StaticClass();
	FDTCombatAnimation CastAnim; CastAnim.Montage = CastMontage;
	Caster->AnimationRouter->CombatAnimations.Add(Fire->AbilityId, CastAnim);

	TestTrue(TEXT("Release notify is authored in the montage"),
		UDTAnimationRouter::FindNotifyTriggerTime(CastMontage, Fire->ReleaseEventTag) > 0.f);

	const auto CountProjectiles = [this, &Context]()
	{
		int32 Count = 0;
		for (TActorIterator<ARPGProjectile> It(Context.World); It; ++It) ++Count;
		return Count;
	};

	URPGAbilitySystemComponent* CasterASC = CastChecked<URPGAbilitySystemComponent>(Caster->GetAbilitySystemComponent());
	const FGameplayAbilitySpecHandle FireHandle = CasterASC->EnsureAbilityGranted(Fire, 1);
	TestTrue(TEXT("Spell receives a real Gameplay Ability spec"), FireHandle.IsValid());
	TestTrue(TEXT("Spell activation commits through GAS"), CasterASC->ActivateAbilityDefinition(Fire, 1, nullptr));
	const FGameplayAbilitySpec* FireSpec = CasterASC->FindAbilitySpecFromHandle(FireHandle);
	TestTrue(TEXT("Deferred spell keeps its Gameplay Ability active before release"), FireSpec && FireSpec->IsActive());
	TestEqual(TEXT("Commit state carries ability identity"), Caster->ActionComp->CommittedAction.AbilityId, Fire->AbilityId);
	TestEqual(TEXT("Mana deducted at commit"), PS->Attributes->GetMana(), 45.f);
	TestEqual(TEXT("No projectile before the release event"), CountProjectiles(), 0);
	TestFalse(TEXT("Re-activation while pending is rejected"), CasterASC->ActivateAbilityDefinition(Fire, 1, nullptr));
	TestEqual(TEXT("Rejected pending activation does not charge mana again"), PS->Attributes->GetMana(), 45.f);

	Caster->ActionComp->HandleAnimationEvent(FGameplayTag::RequestGameplayTag(FName("Event.Animation.ComboWindow"), false));
	TestEqual(TEXT("Unrelated animation event does not release"), CountProjectiles(), 0);
	Caster->ActionComp->HandleAnimationEvent(Fire->ReleaseEventTag);
	TestEqual(TEXT("Release event spawns exactly one projectile"), CountProjectiles(), 1);
	FireSpec = CasterASC->FindAbilitySpecFromHandle(FireHandle);
	TestTrue(TEXT("Release ends the Gameplay Ability instance"), FireSpec && !FireSpec->IsActive());
	Caster->ActionComp->HandleAnimationEvent(Fire->ReleaseEventTag);
	TestEqual(TEXT("Duplicate release event is ignored"), CountProjectiles(), 1);

	ARPGCharacter* Victim = Context.Character();
	const float VictimHealth = Victim->GetRPGAttributes()->GetHealth();
	TestTrue(TEXT("Damage enters through the Gameplay Effect path"), Victim->ReceiveCombatDamage(30.f, Caster, Fire->AbilityId));
	TestTrue(TEXT("Victim health reduced"), Victim->GetRPGAttributes()->GetHealth() < VictimHealth);
	TestFalse(TEXT("Non-lethal hit leaves victim alive"), Victim->bIsDead);

	ARPGPlayerState* PS2 = Context.World->SpawnActor<ARPGPlayerState>();
	ARPGCharacter* Caster2 = Context.Character();
	Caster2->SetPlayerState(PS2); Caster2->Initialize(nullptr);
	Caster2->AnimationRouter->CombatAnimations.Add(Fire->AbilityId, CastAnim);
	URPGAbilitySystemComponent* Caster2ASC = CastChecked<URPGAbilitySystemComponent>(Caster2->GetAbilitySystemComponent());
	const FGameplayAbilitySpecHandle FireHandle2 = Caster2ASC->EnsureAbilityGranted(Fire, 1);
	TestTrue(TEXT("Second caster commits through GAS"), Caster2ASC->ActivateAbilityDefinition(Fire, 1, nullptr));
	const FGameplayAbilitySpec* FireSpec2 = Caster2ASC->FindAbilitySpecFromHandle(FireHandle2);
	TestTrue(TEXT("Second deferred ability is active before cancellation"), FireSpec2 && FireSpec2->IsActive());
	Caster2->ActionComp->CancelPendingRelease(TEXT("test cancellation"));
	FireSpec2 = Caster2ASC->FindAbilitySpecFromHandle(FireHandle2);
	TestTrue(TEXT("Cancellation ends the Gameplay Ability as cancelled"), FireSpec2 && !FireSpec2->IsActive());
	Caster2->ActionComp->HandleAnimationEvent(Fire->ReleaseEventTag);
	TestEqual(TEXT("Cancellation before release spawns no projectile"), CountProjectiles(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGUnifiedCharacterHierarchyTest, "DuTraveller.Restoration.UnifiedCharacterHierarchy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGUnifiedCharacterHierarchyTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Player branch derives from the one shared character base"),
		ARPGPlayerCharacter::StaticClass()->IsChildOf(ARPGCharacter::StaticClass()));
	TestTrue(TEXT("NPC branch derives from the one shared character base"),
		ARPGNPCCharacter::StaticClass()->IsChildOf(ARPGCharacter::StaticClass()));
	TestTrue(TEXT("Enemy derives from the NPC branch"),
		ARPGAICharacter::StaticClass()->IsChildOf(ARPGNPCCharacter::StaticClass()));
	TestTrue(TEXT("Quest giver derives from the NPC branch"),
		ARPGQuestGiver::StaticClass()->IsChildOf(ARPGNPCCharacter::StaticClass()));
	TestTrue(TEXT("Mount derives from the NPC branch"),
		ARPGMountCharacter::StaticClass()->IsChildOf(ARPGNPCCharacter::StaticClass()));
	TestEqual(TEXT("Enemy role is explicit"), GetDefault<ARPGAICharacter>()->NPCRole, ERPGNPCRole::Enemy);
	TestEqual(TEXT("Quest-giver role is explicit"), GetDefault<ARPGQuestGiver>()->NPCRole, ERPGNPCRole::QuestGiver);
	TestEqual(TEXT("Mount role is explicit"), GetDefault<ARPGMountCharacter>()->NPCRole, ERPGNPCRole::Mount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGLiveAssetCombatPipelineTest, "DuTraveller.Restoration.LiveAssetCombatPipeline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGLiveAssetCombatPipelineTest::RunTest(const FString& Parameters)
{
	UClass* PlayerClass = LoadClass<ARPGCharacter>(nullptr,
		TEXT("/Game/Blueprints/Characters/BP_PlayerCharacter.BP_PlayerCharacter_C"));
	if (TestNotNull(TEXT("Tutorial player Blueprint loads"), PlayerClass))
	{
		const ARPGCharacter* Defaults = PlayerClass->GetDefaultObject<ARPGCharacter>();
		TestTrue(TEXT("Tutorial player has authored starting equipment"), !Defaults->StartingEquipment.IsEmpty());
	}
	// Exercises the REAL shipped assets: DA_Player_FireBolt, BP_Projectile_FireBolt,
	// the UEFN cast montage and DA_Player_BasicSlash + the UEFN melee montage.
	FRestorationWorld Context;
	ARPGPlayerState* PS = Context.World->SpawnActor<ARPGPlayerState>();
	ARPGCharacter* Caster = Context.Character();
	Caster->SetPlayerState(PS); Caster->Initialize(nullptr);
	PS->AbilitySystem->SetNumericAttributeBase(URPGAttributeSet::GetManaAttribute(), 50.f);

	URPGCombatAbilityDefinition* Fire = LoadObject<URPGCombatAbilityDefinition>(nullptr,
		TEXT("/Game/Data/Abilities/Player/DA_Player_FireBolt.DA_Player_FireBolt"));
	UAnimMontage* CastMontage = LoadObject<UAnimMontage>(nullptr,
		TEXT("/Game/DTIntegration/Animation/Combat/DT_Standing_1H_Cast_Spell_01_UEFN.DT_Standing_1H_Cast_Spell_01_UEFN"));
	UAnimMontage* SlashMontage = LoadObject<UAnimMontage>(nullptr,
		TEXT("/Game/DTIntegration/Animation/Combat/DT_Standing_Melee_Attack_Horizontal_UEFN.DT_Standing_Melee_Attack_Horizontal_UEFN"));
	URPGCombatAbilityDefinition* Slash = LoadObject<URPGCombatAbilityDefinition>(nullptr,
		TEXT("/Game/Data/Abilities/Player/DA_Player_BasicSlash.DA_Player_BasicSlash"));
	if (!TestNotNull(TEXT("DA_Player_FireBolt loads"), Fire) || !TestNotNull(TEXT("Cast montage loads"), CastMontage) ||
		!TestNotNull(TEXT("Slash montage loads"), SlashMontage) || !TestNotNull(TEXT("DA_Player_BasicSlash loads"), Slash)) return true;

	TestTrue(TEXT("FireBolt carries a serialized release tag"), Fire->ReleaseEventTag.IsValid());
	for (const UAnimMontage* Montage : {CastMontage, SlashMontage})
	{
		TSet<FName> SlotNames;
		bool bHasSequence = false;
		for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
		{
			TestFalse(TEXT("Live combat montage slot names must be unique"), SlotNames.Contains(Track.SlotName));
			SlotNames.Add(Track.SlotName);
			for (const FAnimSegment& Segment : Track.AnimTrack.AnimSegments)
			{
				const UAnimSequenceBase* Sequence = Segment.GetAnimReference();
				TestNotNull(TEXT("Every authored combat segment must have an animation"), Sequence);
				if (Sequence)
				{
					bHasSequence = true;
					TestEqual(TEXT("Segment skeleton must match montage"), Sequence->GetSkeleton(), Montage->GetSkeleton());
				}
			}
		}
		TestTrue(TEXT("Live combat montage must contain an evaluated animation sequence"), bHasSequence);
	}
	TestTrue(TEXT("FireBolt carries a projectile class"), Fire->ProjectileClass != nullptr);
	TestTrue(TEXT("FireBolt projectile is the authoritative RPG projectile"),
		Fire->ProjectileClass->IsChildOf(ARPGProjectile::StaticClass()));

	FDTCombatAnimation CastAnim; CastAnim.Montage = CastMontage;
	Caster->AnimationRouter->CombatAnimations.Add(Fire->AbilityId, CastAnim);

	ARPGCharacter* Victim = Context.Character();
	Victim->FactionId = FName("Enemy");
	Victim->SetActorLocation(FVector(200, 0, 0));

	const auto CountProjectiles = [this, &Context]()
	{
		int32 Count = 0;
		for (TActorIterator<ARPGProjectile> It(Context.World); It; ++It) ++Count;
		return Count;
	};

	TestTrue(TEXT("FireBolt activates against a live target"), Caster->ActionComp->TryActivateAbility(Fire, 1, Victim));
	TestEqual(TEXT("No projectile before the montage release event"), CountProjectiles(), 0);
	Caster->ActionComp->HandleAnimationEvent(Fire->ReleaseEventTag);
	TestEqual(TEXT("Release event spawns exactly one live projectile"), CountProjectiles(), 1);
	Caster->ActionComp->HandleAnimationEvent(Fire->ReleaseEventTag);
	TestEqual(TEXT("Second release event is ignored"), CountProjectiles(), 1);

	// Melee: weapon-overlap path driven by the authored notify window.
	ARPGPlayerState* PS2 = Context.World->SpawnActor<ARPGPlayerState>();
	ARPGCharacter* Swinger = Context.Character();
	Swinger->SetPlayerState(PS2); Swinger->Initialize(nullptr);
	FDTCombatAnimation SlashAnim; SlashAnim.Montage = SlashMontage;
	Swinger->AnimationRouter->CombatAnimations.Add(Slash->AbilityId, SlashAnim);

	ARPGCharacter* MeleeVictim = Context.Character();
	MeleeVictim->FactionId = FName("Enemy");
	MeleeVictim->SetActorLocation(FVector(200, 0, 0));
	const float VictimHealth = MeleeVictim->GetRPGAttributes()->GetHealth();

	TestTrue(TEXT("Melee ability commits"), Swinger->ActionComp->TryActivateAbility(Slash, 1, MeleeVictim));
	TestEqual(TEXT("No damage before the weapon window"), MeleeVictim->GetRPGAttributes()->GetHealth(), VictimHealth);
	Swinger->ActionComp->SetMeleeWindowOpen(true);
	Swinger->ActionComp->HandleWeaponCollision(MeleeVictim);
	TestTrue(TEXT("Weapon overlap inside the window deals damage"),
		MeleeVictim->GetRPGAttributes()->GetHealth() < VictimHealth);
	const float AfterFirstHit = MeleeVictim->GetRPGAttributes()->GetHealth();
	Swinger->ActionComp->HandleWeaponCollision(MeleeVictim);
	TestEqual(TEXT("Duplicate overlap within one attack is deduplicated"),
		MeleeVictim->GetRPGAttributes()->GetHealth(), AfterFirstHit);
	Swinger->ActionComp->SetMeleeWindowOpen(false);
	Swinger->ActionComp->HandleWeaponCollision(MeleeVictim);
	TestEqual(TEXT("Overlap outside the window deals nothing"),
		MeleeVictim->GetRPGAttributes()->GetHealth(), AfterFirstHit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGMountPossessionTest, "DuTraveller.Restoration.MountPossession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGMountPossessionTest::RunTest(const FString& Parameters)
{
	FRestorationWorld Context;
	ARPGPlayerController* PC = Context.World->SpawnActor<ARPGPlayerController>();
	ARPGPlayerState* PS = Context.World->SpawnActor<ARPGPlayerState>();
	ARPGCharacter* Rider = Context.Character();
	PC->PlayerState = PS; PC->Possess(Rider); Rider->SetPlayerState(PS); Rider->Initialize(nullptr);

	ARPGMountCharacter* Mount = Context.World->SpawnActor<ARPGMountCharacter>();
	TestTrue(TEXT("Mount enters the ride interaction"), Mount->TryMount(Rider));
	TestTrue(TEXT("Controller now drives the mount"), PC->GetPawn() == static_cast<APawn*>(Mount));
	TestTrue(TEXT("Rider is parked and hidden"), Rider->IsHidden() && Mount->MountedRider == Rider);
	TestFalse(TEXT("Second mount request is rejected while occupied"), Mount->TryMount(Context.Character()));
	TestTrue(TEXT("Dismount restores the rider"), Mount->TryDismount());
	TestTrue(TEXT("Controller is back on the rider"), PC->GetPawn() == static_cast<APawn*>(Rider));
	TestFalse(TEXT("Rider is visible again"), Rider->IsHidden());
	TestTrue(TEXT("Mount can be re-entered"), Mount->TryMount(Rider));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGSkillTreeProgressionTest, "DuTraveller.Restoration.SkillTreeProgressionAndAllSlots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGSkillTreeProgressionTest::RunTest(const FString& Parameters)
{
	FRestorationWorld Context;
	ARPGPlayerState* PS = Context.World->SpawnActor<ARPGPlayerState>();
	PS->SpellPoints = 20; PS->LVL = 10;
	URPGCombatAbilityDefinition* Lower = NewObject<URPGCombatAbilityDefinition>(PS);
	Lower->AbilityId = TEXT("Abilities.Fire.FireBolt"); Lower->bRequiresTarget = false; Lower->MaximumRank = 3;
	URPGCombatAbilityDefinition* Upper = NewObject<URPGCombatAbilityDefinition>(PS);
	Upper->AbilityId = TEXT("Abilities.Fire.FireBlast"); Upper->bRequiresTarget = false;
	Upper->PrerequisiteAbilities = {Lower->AbilityId};
	PS->Skills->AvailableAbilities = {Lower, Upper};
	TestFalse(TEXT("Upper node cannot unlock before lower node"), PS->Skills->CanLearnAbility(Upper));
	TestFalse(TEXT("Server rejects bypassing the tree"), PS->Skills->TryLearnAbility(Upper));
	TestTrue(TEXT("Lower node learns"), PS->Skills->TryLearnAbility(Lower));
	TestTrue(TEXT("Upper node unlocks after its prerequisite"), PS->Skills->CanLearnAbility(Upper));
	TestTrue(TEXT("Upper node learns"), PS->Skills->TryLearnAbility(Upper));
	TestEqual(TEXT("Two learned nodes cost two points"), PS->SpellPoints, 18);
	for (int32 Slot = 0; Slot < 12; ++Slot)
	{
		TestTrue(FString::Printf(TEXT("Skill can move to action slot %d"), Slot), PS->Skills->TryEquipHotbar(Lower, Slot));
		TestTrue(TEXT("The chosen slot owns the definition"), PS->Skills->Hotbar.ContainsByPredicate([&](const FRPGHotbarSlot& S) { return S.Slot == Slot && S.Definition == Lower; }));
	}
	TestTrue(TEXT("Second skill can occupy LMB"), PS->Skills->TryEquipHotbar(Upper, 0));
	TestTrue(TEXT("Swapping with LMB succeeds"), PS->Skills->TryEquipHotbar(Lower, 0));
	TestTrue(TEXT("Displaced LMB skill moves to old zero-key slot"), PS->Skills->Hotbar.ContainsByPredicate([&](const FRPGHotbarSlot& S) { return S.Slot == 11 && S.Definition == Upper; }));
	TestFalse(TEXT("Out-of-range slot rejected"), PS->Skills->TryEquipHotbar(Lower, 12));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGTargetLockTest, "DuTraveller.Restoration.TargetLockToggle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGTargetLockTest::RunTest(const FString& Parameters)
{
	FRestorationWorld Context(true);
	ARPGPlayerController* PC = Context.World->SpawnActor<ARPGPlayerController>();
	ARPGCharacter* Self = Context.Character();
	PC->Possess(Self);
	Self->SetActorLocation(FVector::ZeroVector);
	PC->SetControlRotation(FRotator::ZeroRotator);
	ARPGCharacter* Enemy = Context.Character(); Enemy->FactionId = TEXT("Enemy");
	Enemy->SetActorLocation(FVector(600,0,0));
	TestTrue(TEXT("Hostile living actor is a lock candidate"), PC->IsTargetLockValid(Enemy));
	TestFalse(TEXT("Cannot lock self"), PC->IsTargetLockValid(Self));
	PC->ToggleTargetLock();
	TestTrue(TEXT("Toggle acquires front enemy"), PC->LockedCombatTarget == Enemy);
	PC->ToggleTargetLock();
	TestNull(TEXT("Second toggle restores free combat"), PC->LockedCombatTarget.Get());
	Enemy->bIsDead = true;
	TestFalse(TEXT("Dead actors cannot be locked"), PC->IsTargetLockValid(Enemy));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGCompleteTreeContentTest, "DuTraveller.Restoration.CompleteTreeContentAndPayloads",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGCompleteTreeContentTest::RunTest(const FString& Parameters)
{
	UClass* StateClass = LoadClass<ARPGPlayerState>(nullptr, TEXT("/Game/Blueprints/Framework/BP_PlayerState.BP_PlayerState_C"));
	if (!TestNotNull(TEXT("Production PlayerState loads"), StateClass)) return false;
	const ARPGPlayerState* State = StateClass->GetDefaultObject<ARPGPlayerState>();
	const TArray<URPGCombatAbilityDefinition*> Definitions = State->Skills->GetAvailableDefinitions();
	TestTrue(TEXT("All 32 tree skills have executable definitions"), Definitions.Num() >= 32);
	UClass* PlayerClass = LoadClass<ARPGCharacter>(nullptr, TEXT("/Game/Blueprints/Characters/BP_PlayerCharacter.BP_PlayerCharacter_C"));
	if (!TestNotNull(TEXT("Production player loads"), PlayerClass)) return false;
	const ARPGCharacter* Player = PlayerClass->GetDefaultObject<ARPGCharacter>();
	for (const URPGCombatAbilityDefinition* D : Definitions)
	{
		TestTrue(D->AbilityId.ToString() + TEXT(" is configured"), D->IsConfigured());
		for (const FName Required : D->PrerequisiteAbilities)
			TestTrue(TEXT("Every prerequisite resolves"), Definitions.ContainsByPredicate([Required](const URPGCombatAbilityDefinition* Other) { return Other->AbilityId == Required; }));
		if (D->Kind != ERPGAbilityKind::Passive)
			TestNotNull(D->AbilityId.ToString() + TEXT(" has a montage"), Player->AnimationRouter->FindCombatMontage(D->AbilityId));
	}
	FRestorationWorld Context(true);
	ARPGCharacter* Caster = Context.Character();
	Caster->SetActorLocation(FVector::ZeroVector);
	UAbilitySystemComponent* ASC = Caster->GetAbilitySystemComponent();
	ASC->SetNumericAttributeBase(URPGAttributeSet::GetHealthAttribute(), 30.f);
	URPGCombatAbilityDefinition* D = NewObject<URPGCombatAbilityDefinition>(Caster);
	D->AbilityId = TEXT("Test.Heal"); D->Execution = ERPGSkillExecution::Heal; D->EffectMagnitude = 20.f;
	URPGSkillExecution::Execute(Caster, D, nullptr, 0.f, 1);
	TestEqual(TEXT("Healing restores health through GAS"), Caster->GetRPGAttributes()->GetHealth(), 50.f);
	D->Execution = ERPGSkillExecution::Shield; D->EffectMagnitude = 30.f;
	const float Armor = Caster->GetRPGAttributes()->GetArmor();
	URPGSkillExecution::Execute(Caster, D, nullptr, 0.f, 1);
	TestEqual(TEXT("Defensive skill applies armor modifier"), Caster->GetRPGAttributes()->GetArmor(), Armor + 30.f);
	ARPGCharacter* Enemy = Context.Character(); Enemy->FactionId = TEXT("Enemy"); Enemy->SetActorLocation(FVector(100,0,0));
	D->Execution = ERPGSkillExecution::Cone; D->Range = 300.f;
	const float Before = Enemy->GetRPGAttributes()->GetHealth();
	URPGSkillExecution::Execute(Caster, D, nullptr, 15.f, 1);
	TestTrue(TEXT("Free-aim cone damages hostile actor in front"), Enemy->GetRPGAttributes()->GetHealth() < Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGInventoryEquipmentDropTest, "DuTraveller.Restoration.InventoryEquipmentDragDrop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGInventoryEquipmentDropTest::RunTest(const FString& Parameters)
{
	FRestorationWorld Context;
	ARPGPlayerController* PC = Context.World->SpawnActor<ARPGPlayerController>();
	ARPGCharacter* Pawn = Context.Character();
	PC->Possess(Pawn);
	PC->SetPlayer(NewObject<ULocalPlayer>(GEngine));
	URPG_UserWidget* TargetWidget = NewObject<URPG_UserWidget>();
	TargetWidget->SetOwningPlayer(PC);
	Pawn->InventoryComp->TryAddItemFromClass(UGear_ItemObject::StaticClass(), 2);
	UGear_ItemObject* First = CastChecked<UGear_ItemObject>(Pawn->InventoryComp->GetItem(0));
	UGear_ItemObject* Second = CastChecked<UGear_ItemObject>(Pawn->InventoryComp->GetItem(1));
	First->Slot = Second->Slot = EEquipableSlot::EIS_Helmet;
	First->AttributeBonuses = {{FName("Strength"), 3.f}, {FName("MaxMana"), 20.f}};
	Second->AttributeBonuses = {{FName("Strength"), 7.f}};
	UAbilitySystemComponent* ASC = Pawn->GetAbilitySystemComponent();
	ASC->SetNumericAttributeBase(URPGAttributeSet::GetStrengthAttribute(), 10.f);
	UDTInventoryDragDrop* Operation = NewObject<UDTInventoryDragDrop>();
	Operation->DraggedItem = First;
	TestFalse(TEXT("Wrong equipment slot rejected"), UDTInventoryDragDrop::ApplyEquipmentDrop(TargetWidget, Operation, EEquipableSlot::EIS_Feet));
	TestEqual(TEXT("Rejected drop leaves stats unchanged"), Pawn->GetRPGAttributes()->GetStrength(), 10.f);
	TestTrue(TEXT("Character drop equips to the item's own slot"), UDTInventoryDragDrop::ApplyEquipmentDrop(TargetWidget, Operation));
	TestEqual(TEXT("Drop applies real GAS stat bonus"), Pawn->GetRPGAttributes()->GetStrength(), 13.f);
	TestEqual(TEXT("Drop applies max-resource bonus"), Pawn->GetRPGAttributes()->GetMaxMana(), 70.f);
	TestFalse(TEXT("Equipped item leaves the bag presentation"), First->ShouldShowInInventory());
	TestTrue(TEXT("Repeated drop is idempotent"), UDTInventoryDragDrop::ApplyEquipmentDrop(TargetWidget, Operation));
	TestEqual(TEXT("Repeated drop does not stack bonuses"), Pawn->GetRPGAttributes()->GetStrength(), 13.f);
	Operation->DraggedItem = Second;
	Second->AttributeBonuses.Add(TEXT("InvalidAttribute"), 1.f);
	TestFalse(TEXT("Invalid stat data cannot replace working equipment"), UDTInventoryDragDrop::ApplyEquipmentDrop(TargetWidget, Operation));
	TestTrue(TEXT("Rejected swap keeps original item equipped"), Pawn->IsItemEquipped(First));
	Second->AttributeBonuses.Remove(TEXT("InvalidAttribute"));
	TestTrue(TEXT("Same-slot drop replaces old gear"), UDTInventoryDragDrop::ApplyEquipmentDrop(TargetWidget, Operation));
	TestEqual(TEXT("Swap replaces bonuses"), Pawn->GetRPGAttributes()->GetStrength(), 17.f);
	TestTrue(TEXT("Old item returns to bag presentation"), First->ShouldShowInInventory());
	TestEqual(TEXT("Old maximum bonus removed"), Pawn->GetRPGAttributes()->GetMaxMana(), 50.f);
	TestTrue(TEXT("Drop back to bag unequips"), UDTInventoryDragDrop::ApplyEquipmentDrop(TargetWidget, Operation, EEquipableSlot::EIS_None, true));
	TestEqual(TEXT("Unequip preserves permanent base"), Pawn->GetRPGAttributes()->GetStrength(), 10.f);
	ARPGCharacter* Other = Context.Character();
	Other->InventoryComp->TryAddItemFromClass(UGear_ItemObject::StaticClass(), 1);
	Operation->DraggedItem = Other->InventoryComp->GetItem(0);
	TestFalse(TEXT("Foreign inventory item rejected"), UDTInventoryDragDrop::ApplyEquipmentDrop(TargetWidget, Operation));
	UDragDropOperation* Legacy = NewObject<UDragDropOperation>(); Legacy->Payload = First;
	TestTrue(TEXT("Existing item payload remains supported"), UDTInventoryDragDrop::ResolveItem(Legacy) == First);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGEnemyTargetFilteringTest, "DuTraveller.Restoration.EnemyTargetFiltering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGEnemyTargetFilteringTest::RunTest(const FString& Parameters)
{
	FRestorationWorld Context;
	ARPGAICharacter* Enemy = Context.World->SpawnActor<ARPGAICharacter>();
	Enemy->Initialize(nullptr);
	ARPGAIController* Controller = Context.World->SpawnActor<ARPGAIController>();
	Controller->Possess(Enemy);
	ARPGCharacter* Neutral = Context.Character(); Neutral->FactionId = TEXT("Neutral"); Neutral->SetActorLocation(FVector(30,0,0));
	ARPGCharacter* Hero = Context.Character(); Hero->FactionId = TEXT("Player"); Hero->SetActorLocation(FVector(100,0,0));
	TestTrue(TEXT("Enemy ignores the nearer neutral NPC"), Controller->AcquireCombatTarget() == Hero);
	Hero->bIsDead = true;
	TestNull(TEXT("Dead heroes and neutral NPCs are not combat targets"), Controller->AcquireCombatTarget());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGDeathRespawnTest, "DuTraveller.Restoration.DeathAndRespawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGDeathRespawnTest::RunTest(const FString& Parameters)
{
	FRestorationWorld Context;
	ARPGCharacter* Pawn = Context.Character();
	Pawn->InventoryComp->TryAddItemFromClass(UGear_ItemObject::StaticClass(), 1);
	UEquipment_ItemObject* Gear = Cast<UEquipment_ItemObject>(Pawn->InventoryComp->GetItem(0));
	Gear->Slot = EEquipableSlot::EIS_Helmet;
	Gear->AttributeBonuses.Add(TEXT("Strength"), 7.f);
	const EEquipableSlot Slot = Gear->Slot;
	TestTrue(TEXT("Gear equips before death"), Pawn->TryEquipItem(Gear, Slot));
	const float Strength = Pawn->GetRPGAttributes()->GetStrength();
	TestTrue(TEXT("Lethal GAS damage is accepted"), Pawn->ReceiveCombatDamage(1000.f, nullptr, TEXT("Test.Fatal")));
	TestTrue(TEXT("Lethal health marks death"), Pawn->bIsDead);
	TestEqual(TEXT("Dead health remains zero"), Pawn->GetRPGAttributes()->GetHealth(), 0.f);
	TestFalse(TEXT("Dead pawn cannot receive more damage"), Pawn->ReceiveCombatDamage(1.f, nullptr, TEXT("Test.Fatal")));
	TestTrue(TEXT("Authority can respawn at a start transform"), Pawn->RespawnAt(FTransform(FVector(200,100,100))));
	TestFalse(TEXT("Respawn clears death"), Pawn->bIsDead);
	TestEqual(TEXT("Respawn fills health"), Pawn->GetRPGAttributes()->GetHealth(), Pawn->GetRPGAttributes()->GetMaxHealth());
	TestEqual(TEXT("Respawn retains inventory identity"), Pawn->InventoryComp->GetItem(0), static_cast<UItemObject*>(Gear));
	TestTrue(TEXT("Respawn retains equipped item"), Pawn->IsItemEquipped(Gear));
	TestEqual(TEXT("Respawn does not duplicate gear stats"), Pawn->GetRPGAttributes()->GetStrength(), Strength);
	TestFalse(TEXT("A live pawn cannot request another respawn"), Pawn->RespawnAt(FTransform::Identity));
	Pawn->GetAbilitySystemComponent()->SetNumericAttributeBase(URPGAttributeSet::GetHealthAttribute(), 0.f);
	TestTrue(TEXT("Direct GAS health changes also enter death"), Pawn->bIsDead);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGGroundedDeathLootTest, "DuTraveller.Restoration.GroundedDeathLoot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGGroundedDeathLootTest::RunTest(const FString& Parameters)
{
	FRestorationWorld Context(true);
	AStaticMeshActor* Floor = Context.World->SpawnActor<AStaticMeshActor>();
	Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Floor->SetActorScale3D(FVector(20,20,.1f));
	Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	Context.World->Tick(LEVELTICK_All,.02f);
	ARPGAICharacter* Enemy = Context.World->SpawnActor<ARPGAICharacter>(FVector(0,0,100),FRotator::ZeroRotator);
	Enemy->LootItemClass = UItemObject::StaticClass();
	Enemy->bIsDead = true;
	Enemy->SpawnDeathLoot();
	Enemy->SpawnDeathLoot();
	ARPGPickupItem* Loot = nullptr;
	int32 Count = 0;
	for (TActorIterator<ARPGPickupItem> It(Context.World); It; ++It)
		if (It->GetOwner()==Enemy) { Loot=*It; ++Count; }
	TestEqual(TEXT("Repeated death reward request creates one pickup"),Count,1);
	if (!TestNotNull(TEXT("Native fallback pickup works without a pickup Blueprint"),Loot)) return false;
	FVector Center, Extent; Loot->GetActorBounds(false,Center,Extent);
	TestTrue(TEXT("Mesh bottom is grounded instead of hovering at capsule height"),FMath::IsNearlyEqual(Center.Z-Extent.Z,7.f,.1f));
	const float GroundedZ = Loot->GetActorLocation().Z;
	TestTrue(TEXT("Ground placement can be repeated"),Loot->PlaceOnGround());
	TestTrue(TEXT("Repeated ground placement does not accumulate a height offset"),FMath::IsNearlyEqual(Loot->GetActorLocation().Z,GroundedZ,.1f));
	ARPGCharacter* Collector = Context.Character(); Collector->SetActorLocation(FVector(100,0,100));
	TestTrue(TEXT("Grounded death loot can enter inventory"),Loot->TryCollect(Collector));
	TestNotNull(TEXT("Collected loot retains its item class"),Collector->InventoryComp->FindItemByClass(UItemObject::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGThreatPredictionTest, "DuTraveller.Restoration.IncomingThreatPrediction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRPGThreatPredictionTest::RunTest(const FString& Parameters)
{
	FRestorationWorld Context(true);
	ARPGPlayerCharacter* Hero=Context.World->SpawnActor<ARPGPlayerCharacter>(FVector(0,0,90),FRotator::ZeroRotator);
	Hero->Initialize(nullptr);
	ARPGCharacter* Enemy=Context.Character(); Enemy->FactionId=TEXT("Enemy"); Enemy->SetActorLocation(FVector(1000,0,90));
	URPGCombatAbilityDefinition* Definition=NewObject<URPGCombatAbilityDefinition>(); Definition->ProjectileSpeed=1000.f;
	ARPGProjectile* Shot=Context.World->SpawnActor<ARPGProjectile>(FVector(300,0,90),FRotator(0,180,0));
	Shot->InitializeProjectile(Enemy,Definition,10.f,1);
	AActor* Threat=nullptr; float Time=-1;
	TestTrue(TEXT("Incoming hostile trajectory predicts a near hit"),Hero->FindIncomingThreat(Threat,Time));
	TestTrue(TEXT("Prediction identifies this projectile and a positive reaction lead"),Threat==Shot && Time>=0 && Time<Hero->ThreatLeadSeconds);
	Shot->SetActorLocation(FVector(300,200,90));
	TestFalse(TEXT("A close projectile passing to the side does not trigger focus"),Hero->FindIncomingThreat(Threat,Time));
	Shot->SetActorLocation(FVector(300,0,90)); Shot->Movement->Velocity=FVector(1000,0,0);
	TestFalse(TEXT("Receding projectiles are ignored"),Hero->FindIncomingThreat(Threat,Time));
	Shot->Movement->Velocity=FVector(-1000,0,0); Enemy->FactionId=TEXT("Player");
	TestFalse(TEXT("Friendly projectiles are ignored"),Hero->FindIncomingThreat(Threat,Time));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRestoredXPLevelTransitions, "DuTraveller.Restoration.XPLevelTransitions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRestoredXPLevelTransitions::RunTest(const FString& Parameters)
{
	FRestorationWorld Fixture;
	ARPGPlayerState* PS=Fixture.World->SpawnActor<ARPGPlayerState>();
	URPGOverlayWidgetController* Overlay=NewObject<URPGOverlayWidgetController>();
	FWidgetControllerParams Params; Params.PlayerState=PS; Params.AbilitySystemComponent=PS->AbilitySystem; Params.AttributeSet=PS->Attributes;
	Overlay->SetWidgetControllerParams(Params); Overlay->BroadcastInitialValues();
	UDTProgressionEventProbe* Probe=UDTStructRepairLibrary::ObserveProgression(PS,Overlay);
	if (!TestNotNull(TEXT("Progression observer"),Probe)) return false;
	const int32 InitialAttributes=PS->AttributePoints, InitialSkills=PS->SpellPoints;
	TestTrue(TEXT("Partial XP award succeeds"),PS->GrantExperience(24.f,false));
	TestEqual(TEXT("Partial XP stays in level one"),PS->LVL,1);
	TestEqual(TEXT("XP percentage reflects the authoritative value"),PS->GetExperiencePercent(),.24f);
	TestEqual(TEXT("Partial XP emits no level-up event"),Probe->PlayerStateLevels.Num(),0);
	TestTrue(TEXT("Exact threshold award succeeds"),PS->GrantExperience(76.f,false));
	TestEqual(TEXT("A full bar raises the level"),PS->LVL,2);
	TestEqual(TEXT("Exact threshold leaves zero XP"),PS->EXP,0.f);
	TestEqual(TEXT("Exact threshold emits one event"),Probe->PlayerStateLevels.Num(),1);
	TestTrue(TEXT("Multi-level award succeeds"),PS->GrantExperience(250.f,false));
	TestEqual(TEXT("One large award crosses both thresholds"),PS->LVL,4);
	TestEqual(TEXT("Overflow XP is preserved"),PS->EXP,50.f);
	TestTrue(TEXT("Canonical level events fire exactly once per gained level"),Probe->PlayerStateLevels==TArray<int32>({2,3,4}));
	TestTrue(TEXT("HUD level-up flags fire exactly once per gained level"),Probe->OverlayLevels==TArray<int32>({2,3,4}));
	TestEqual(TEXT("Attribute rewards match all three levels"),PS->AttributePoints,InitialAttributes+3*PS->AttributePointsPerLevel);
	TestEqual(TEXT("Skill rewards match all three levels"),PS->SpellPoints,InitialSkills+3*PS->SkillPointsPerLevel);
	PS->GrantProgressionPoints(5,2); PS->OnRep_Progression(); Overlay->BroadcastInitialValues(); PS->GrantExperience(0,false);
	TestEqual(TEXT("Stat updates/rebinding/zero XP emit no further level events"),Probe->PlayerStateLevels.Num(),3);
	TestEqual(TEXT("HUD rebind emits no further level-ups"),Probe->OverlayLevels.Num(),3);
	TestFalse(TEXT("Negative XP is rejected"),PS->GrantExperience(-1,false));
	return true;
}

#endif
