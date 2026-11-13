# DuTraveller UE 5.8 Migration

## Objective

Upgrade the complete UE 5.4.2 project to UE 5.8 without changing intended gameplay behavior. The historical repository and its `master` branch remain the source baseline. Compatibility work is performed on `migration/ue5.8-parity` and targets `garodon21aque/DuTraveller`.

## Compatibility invariants

- Preserve the `DT` module name, the legacy `/Script/DuTraveller` identity, and existing `/Game` asset paths during the parity pass.
- Preserve Blueprint parent classes, interfaces, serialized references, Gameplay Tags, and input mappings.
- Preserve quest progression, rewards, objective state, and persistence.
- Preserve character-class identity, starting state, progression, and class-specific abilities.
- Preserve GAS ability grants, effects, attributes, tags, costs, cooldowns, targeting, and activation behavior.
- Preserve spell unlock, equip, cast, cooldown, presentation, and UI flows.
- Preserve inventory, item stacking, equipment, drag/drop, vendors, crafting, and persistence.
- Preserve widget behavior, layout, focus, input mode, world widgets, and HUD feedback.
- Preserve maps, World Partition external actors, animations, Niagara effects, audio, AI, and interactions.
- Keep compatibility changes separate from optional modernization.

## Baseline inventory

- Source commit: `f90ae6dcd48378bf890cbada7d893315c86c6fe8`
- Content files: 3,660 (7.64 GB materialized through Git LFS)
- Unreal assets: 3,646 `.uasset` files
- Maps: 14 `.umap` files
- Native code: 19 headers and 19 implementation files
- Primary game maps: `Content/Levels/MainMenu.umap` and `Content/Levels/Tutorial.umap`
- Primary UI groups: Attribute, CraftingSystem, Dialogs, HouseSystem, HUD, InventorySystem, Menu, QuestSystem, SpellMenu, and World

## Source-integrity finding

The GitHub snapshot is not a self-contained build of the historical game. It contains one commit and no alternate branches or tags. The checked-in `DT` module is mostly class scaffolding, while shipped assets still serialize native parents and APIs from a missing module named `DuTraveller`.

Direct legacy native parents found in assets:

- `RPG_UserWidget`
- `RPGWorldUserWidget`
- `RPGAnimInstance`
- `RPG_ItemWidget`
- `RPG_ItemToolTip`
- `RPG_DamageTextComponent`
- `RPG_PlayerStatWidget`
- `RPG_TrainerWidget`

The first UE 5.8 Blueprint pass reported 277 fatal issues because these parents could not load. A thin compatibility module restored their exact script identities and compiled successfully. The next pass could load every Blueprint, but exposed the larger omission: 56 project Blueprints produced 1,204 fatal compiler issues because their historical controller, struct, delegate, inventory, quest, and animation APIs were not committed. This is a source-baseline defect rather than an engine-upgrade regression.

The compatibility module is intentionally behavior-free. It is a diagnostic bridge and must not be treated as a completed gameplay implementation.

## Required plugins

All plugins below were found in the installed UE 5.8 engine:

- GameplayAbilities
- MotionTrajectory
- PoseSearch
- MotionWarping
- AnimationWarping
- Chooser
- AnimationLocomotionLibrary
- DeformerGraph
- GameplayStateTree
- NetworkPrediction
- TargetingSystem
- Water
- ModelingToolsEditorMode (editor only)

## Migration gates

- [x] Create isolated clone from the historical source commit.
- [x] Create `migration/ue5.8-parity` without modifying source `master`.
- [x] Materialize and verify all Git LFS content (`git lfs fsck`).
- [x] Confirm all declared plugins exist in UE 5.8.
- [x] Compile the native `DTEditor` target with UE 5.8.
- [x] Run an unattended editor startup and asset-registry scan.
- [x] Compile/load every project Blueprint and record the initial failure set.
- [x] Restore the missing `/Script/DuTraveller` parent identities with a compilable compatibility module.
- [x] Search all currently accessible recovery locations for the original historical `DuTraveller` native source; no usable copy was recovered.
- [x] Freeze Phase 2 evidence in a reproducible serialized-type and Blueprint-contract inventory.
- [ ] Otherwise reconstruct the missing native contracts subsystem by subsystem from asset metadata and known behavior.
- [ ] Reach zero fatal project-Blueprint compile errors before saving upgraded assets.
- [x] Load Main Menu and Tutorial into play under UE 5.8 without saving assets.
- [ ] Complete interactive Main Menu and Tutorial gameplay validation.
- [ ] Validate GAS and spell behavior.
- [ ] Validate quests and rewards.
- [ ] Validate inventory, equipment, crafting, and vendors.
- [ ] Validate HUD, menus, world widgets, keyboard, mouse, and gamepad flows.
- [ ] Validate animation, AI, navigation, targeting, VFX, and audio.
- [ ] Validate save creation, reload, version handling, and persistence.
- [ ] Produce a Development package and run its smoke test.
- [ ] Produce a Shipping package and run its smoke test.
- [ ] Push the verified migration branch to `garodon21aque/DuTraveller`.

## Recovery-first execution plan

1. **Recover before recreating.** Search local backups, old project folders, archives, and GitHub for the omitted `DuTraveller` source. Original source is the only reliable way to guarantee exact behavior.
2. **Freeze the evidence.** Keep the 5.4.2 repository and LFS objects immutable; retain compile logs and an inventory of serialized native types, functions, structs, delegates, Blueprint classes, and dependencies.
3. **Restore native contracts.** Replace the diagnostic compatibility shells with the original classes, or reconstruct exact reflected names and signatures. Start with shared widget/controller types, then GAS/spells, inventory/items, quests, and animation state.
4. **Repair dependency roots first.** Compile foundational classes before leaf widgets so one missing type is not counted as dozens of downstream node errors.
5. **Upgrade assets in controlled batches.** Load, compile, save, and rescan one subsystem at a time. Never bulk-resave while parent classes or serialized types are unresolved.
6. **Validate runtime behavior.** Run the focused acceptance checks below in PIE and standalone, including replication paths where applicable.
7. **Validate maps and packaging.** Load Main Menu and Tutorial, run Development and Shipping packages, and record smoke-test results.
8. **Publish only verified work.** Push the migration branch to the empty destination repository after native build, Blueprint compile, map load, and package gates pass.

### Phase 1 outcome: historical source recovery

Phase 1 is complete. The omitted native implementation was not found in any currently accessible source:

- The four DuTraveller-named GitHub repositories were inspected. `DuTraveller-5.4.2` contains the incomplete one-commit snapshot; `DuTraveller`, `DuTraveller_23`, and `DuTravellerS` contain no recoverable historical source.
- Local project, Documents, Downloads, temporary, and Codex-storage searches found no `Source.zip` or extracted historical `Source/DuTraveller` tree.
- `DuTraveller_UMG_Kit_v1.zip` contains UI artwork and import tooling, not gameplay source.
- The connected Drive project and exact reflected-class searches contain no source archive or individual C++ files.
- A 12 GB local `CascadeProjects.zip` produced no matching DuTraveller, Unreal project, or reflected-class filenames during a read-only archive listing; its malformed unnamed entries make it unsuitable as positive recovery evidence.

Two useful pieces of evidence were recovered even though the code itself was not:

- `DuTraveller_Master_Prompt_v2.md` records that a supplied `Source.zip` once contained the full `Source/DuTraveller` tree. It identifies implemented families including `UItemObject` and its subclasses, `URPGQuest`, `URPGQuestComponent`, `UQuestDataAsset`, `UItemBlueprint`, `URPGCraftingComponent`, `ARPGBuilding`, item/equipment enums, quest enums, and damage/resistance Gameplay Tags. These names are evidence for the Phase 2 contract inventory, but the prose is not a substitute for exact reflected signatures or behavior.
- Google Drive contains a 4.37 GB `DuTraveller Alpha/Project/Windows.rar` packaged build dated August 2024. It may become a behavioral reference in a later validation phase, but a packaged build is not treated as recoverable editable source.

Exact parity therefore cannot be claimed from the repository alone. The migration proceeds by reconstructing serialized contracts from project assets, compile diagnostics, design evidence, and any observable packaged-build behavior, with uncertainty documented rather than guessed.

### Phase 2 outcome: frozen contract evidence

Phase 2 is complete. `Tools/Analyze-BlueprintContracts.ps1` reproducibly parses the preserved UE 5.8 compatibility compile log and writes the inventory under `Docs/BlueprintContractInventory/`.

Authoritative commandlet totals:

- 56 failed Blueprints
- 1,204 fatal Blueprint issues
- 186 warnings attached to those failed Blueprints
- 1,509 parsed Blueprint compiler diagnostic records

Contract candidates recovered from those diagnostics:

- 37 distinct missing reflected function names
- 37 distinct missing reflected property names
- 22 delegate/event names whose signatures can no longer resolve
- 24 distinct serialized field/pin names retained by unknown struct nodes
- 136 distinct missing node/pin pairs, preserving evidence for later signature reconstruction

A binary scan of all original assets also recovered 13 exact project-native serialized identities: five valid `/Script/DT.*` types and eight missing `/Script/DuTraveller.*` types. The latter are the direct legacy parent/component identities already restored by the diagnostic compatibility module. No additional `/Script/DuTraveller.*` identity appears in the materialized asset set.

The largest fatal-error clusters are inventory/equipment/vendors (390), GAS/spells (258), HUD (174), quests (116), GAS/attributes (79), world widgets/effects (69), crafting (56), legacy animation (56), and menus (6). These totals establish the Phase 3 repair order; they do not imply that each compiler symptom is a separate missing native declaration.

The inventory deliberately distinguishes evidence from inference. A retained node name or pin proves that a reflected contract existed, but does not by itself prove the owning class, C++ type, default value, authority/replication policy, or runtime behavior.

### Phase 3 checkpoint 1: shared widget base contract

The first reconstructed contract is the legacy `URPG_UserWidget` controller bridge. Serialized Blueprint evidence and the matching public GAS-RPG ancestry agree on the same three reflected members: `SetWidgetController(UObject*)`, the Blueprint-readable `WidgetController` object property, and the `WidgetControllerSet` Blueprint event. The setter assigns the controller and immediately invokes the event.

The UE 5.8 native build succeeds after this restoration. A fresh full Blueprint commandlet pass is preserved in `Saved/Logs/CompileAllBlueprints-5.8-phase3-widget-base.log`, with its parsed inventory under `Docs/BlueprintContractInventory/Phase3-WidgetBase/`.

Measured change from the frozen Phase 2 baseline:

- Failed Blueprints: 56 -> 52 (4 repaired)
- Fatal issues: 1,204 -> 1,109 (95 removed)
- Warnings attached to failed Blueprints: 186 -> 160 (26 removed)
- Explicit missing-`SetWidgetController` diagnostics: 34 -> 0
- Newly compiling assets: `WBP_EquippedSpell_Row`, `WBP_MageSpellTree`, `WBP_MageTalentTree`, and `WBP_WarriorSpellTree`

This checkpoint validates the dependency-root strategy: one exact base contract repaired four leaf widgets and removed many downstream compiler symptoms without editing or resaving any asset. Phase 3 continues with controller and shared data contracts before subsystem-specific UI leaves.

### Phase 3 checkpoint 2: attribute controller contracts

Binary asset strings resolved the project's exact reflected names, which differ from the public GAS-RPG ancestor in several places. DuTraveller expects `URPGWidgetController`, `URPGAttributeMenuWidgetController`, `FRPG_SAttributeInfo`, and `URPG_AbilitySystemLibrary`. The matching base parameter struct, attribute-info fields, controller delegates, `BroadcastInitialValues`, `UpgradeAttribute`, and `GetAttributeWidgetController` entry point were restored under those exact identities.

The UE 5.8 native build succeeds. The comparison pass is preserved in `Saved/Logs/CompileAllBlueprints-5.8-phase3-controller-base.log`, with parsed output under `Docs/BlueprintContractInventory/Phase3-ControllerBase/`.

Measured change from checkpoint 1:

- Failed Blueprints: 52 -> 50 (2 additional repairs)
- Fatal issues: 1,109 -> 1,052 (57 additional issues removed)
- Warnings attached to failed Blueprints: 160 -> 157 (3 additional warnings removed)
- Newly compiling assets: `WBP_TextValueButtonROw` and `WBP_TextValueRow`
- No new failing Blueprint was introduced

The remaining two errors in `WBP_AttributeMenu` identify the delegate parameter display name as `New Value`; this is a reflected-signature detail and is corrected to `NewValue` before the next pass. `WBP_AttributesPointsRow` additionally proves that three referenced controller Blueprint classes are absent from the repository: `BP_RPGAttributeWidgetController`, `BP_RPGSpellWidgetController`, and `BP_RPGOverlayWidgetController`. Their native bases and serialized members must be restored before redirects or replacement assets can be validated.

### Phase 3 checkpoint 3: GAS spell/overlay UI roots

The serialized GAS UI contracts now include `FRPGAbilityInfo`, `URPGOverlayWidgetController`, and `URPG_SpellMenuWidgetController`, with their exact retained delegate type names, properties, functions, and parameter names. `FOnPlayerStatChangedSignature` replaces the initially inferred delegate identity because the assets explicitly retain `OnPlayerStatChangedSignature__DelegateSignature` and a `New Value` pin.

The three controller Blueprint classes referenced throughout the UI are absent from the repository. Narrow `CoreRedirects` map only those exact missing generated-class paths to their reconstructed native equivalents. This preserves the old cast targets without generating replacement binary assets. Both the UE 5.8 native build and redirect loading succeed.

The comparison pass is preserved in `Saved/Logs/CompileAllBlueprints-5.8-phase3-gas-ui-base.log`, with parsed output under `Docs/BlueprintContractInventory/Phase3-GasUIBase/`.

Measured change from checkpoint 2:

- Failed Blueprints: 50 -> 49 (1 additional repair)
- Fatal issues: 1,052 -> 904 (148 additional issues removed)
- Warnings attached to failed Blueprints: unchanged at 157
- Newly compiling asset: `WBP_AttributeMenu`
- No new failing Blueprint was introduced

Remaining GAS/HUD leaf totals also fell: `WBP_AttributesPointsRow` 15 -> 9, `WBP_SpellGlobe` 36 -> 21, `WBP_SpellGlobe_Button` 47 -> 22, `WBP_SpellMenu` 29 -> 15, `WBP_HealthManaSpells` 9 -> 5, and `WBP_HUD` 32 -> 24 fatal issues. These assets remain blocked by player-state, async cooldown, and other downstream contracts; the controller roots themselves are now validated by the measured reduction.

### Phase 3 checkpoint 4: recreated controller Blueprint identities

Class redirects cannot fully reconstruct serialized Blueprint cast-node targets. The temporary redirects were removed and Unreal generated three minimal Blueprint assets at their original paths, parented to the validated native contracts:

- `/Game/Blueprints/Controllers/BP_RPGAttributeWidgetController`
- `/Game/Blueprints/Controllers/BP_RPGOverlayWidgetController`
- `/Game/Blueprints/Controllers/BP_RPGSpellWidgetController`

`Tools/Create-ControllerBlueprints.py` makes this repair repeatable and refuses to replace existing assets. The resulting `.uasset` files are generated and saved by UE 5.8, not manually synthesized.

The comparison pass is preserved in `Saved/Logs/CompileAllBlueprints-5.8-phase3-controller-assets.log`, with parsed output under `Docs/BlueprintContractInventory/Phase3-ControllerAssets/`.

Measured change from checkpoint 3:

- Failed Blueprints: 49 -> 38 (11 additional repairs)
- Fatal issues: 904 -> 777 (127 additional issues removed)
- Warnings attached to failed Blueprints: unchanged at 157
- Newly compiling assets: `WBP_AttributesPointsRow`, `WBP_ExperienceGlobe`, `WBP_HealthGlobe`, `WBP_Level`, `WBP_ManaGlobe`, `WBP_SpellMenu`, `WBP_Experience`, `WBP_Health`, `WBP_HealthManaSpells`, `WBP_Mana`, and `WBP_Stamina`
- No new failing Blueprint was introduced

Only three GAS/HUD roots remain in this cluster: `WBP_SpellGlobe` (14 fatal), `WBP_SpellGlobe_Button` (5), and `WBP_HUD` (18). Their remaining shared blockers are the serialized `Info` parameter name on `FAbilityInfoSignature`, the missing cooldown async task, and player-controller/HUD-specific dependencies.

### Phase 3 checkpoint 5: cooldown observation and spell globe

`URPG_WaitCooldownChange` restores the Blueprint async action retained by the spell-globe graph. It validates the ASC and cooldown tag, observes tag-count and active-effect changes, queries the longest matching cooldown, broadcasts `CooldownStart`/`CooldownEnd`, and cleanly unregisters through `EndTask`. The retained `FAbilityInfoSignature` parameter is also named `Info`, matching the serialized event pin.

The UE 5.8 build succeeds. The comparison pass is preserved in `Saved/Logs/CompileAllBlueprints-5.8-phase3-cooldown-task.log`, with parsed output under `Docs/BlueprintContractInventory/Phase3-CooldownTask/`.

Measured change from checkpoint 4:

- Failed Blueprints: 38 -> 37 (1 additional repair)
- Fatal issues: 777 -> 763 (14 additional issues removed)
- Warnings attached to failed Blueprints: 157 -> 156
- Newly compiling asset: `WBP_SpellGlobe`
- No new failing Blueprint was introduced

`WBP_SpellGlobe_Button` now proves `AbilityInfoDelegate` belongs to the common `URPGWidgetController` base so both overlay and spell controllers expose it. `WBP_HUD` is no longer blocked by shared widget-controller contracts; its remaining errors belong to the missing player-controller/input and HUD-specific layer.

### Phase 3 checkpoint 6: shared spell-controller delegate

`FAbilityInfoSignature AbilityInfoDelegate` now lives on `URPGWidgetController`, matching the common base retained by both overlay and spell-controller graphs. A focused compile of `/Game/UI/HUD/SpellGlobes/SpellBoard/WBP_SpellGlobe_Button` completes with zero Blueprint compiler errors and zero Blueprint compiler warnings. This closes the shared GAS/spell UI root; `WBP_HUD` remains a separate player-controller/input restoration task.

The focused load also exposed four ability-status tags that were serialized in graph defaults but absent from project configuration. `Abilities.Status.Eligible`, `Abilities.Status.Equipped`, `Abilities.Status.Locked`, and `Abilities.Status.Unlocked` are now registered in `DefaultGameplayTags.ini`. The prior focused run is preserved in `Saved/Logs/Compile-WBP_SpellGlobe_Button.log`; the tag registrations will be verified on the next UE process because tag configuration is loaded at startup.

This was a focused acceptance check rather than a full-project comparison pass, so the authoritative project totals remain checkpoint 5's 37 failed Blueprints, 763 fatal issues, and 156 warnings until the next full compile.

### Phase 3 checkpoint 7: item and inventory foundation

The preserved UI and the project's historical design notes identify the old pre-rename Narrative Inventory lineage under DuTraveller's local identities. `UItemObject`, `UInventoryComponent`, `FItemAddResult`, `EItemAddResult`, `EItemRarity`, and `EEquipableSlot` are restored with the exact names retained by the assets. This is a functional foundation rather than a signature-only shell: it supports bounded inventory capacity, stack-aware addition, item lookup, quantity mutation, use/removal, and update notifications.

The legacy item-widget bases now expose typed `Item`, `Inventory`, and `InvItemWidget` spawn properties. `UItemObject` exposes its retained display name, description, thumbnail, rarity, price, stackability, quantity, owner, tooltip class, and `OnItemModified` delegate. The native UE 5.8 build succeeds; its log is preserved at `Saved/Logs/Build-phase3-inventory-foundation.log`.

A focused comparison pass is preserved at `Saved/Logs/Compile-InventoryFoundation.log`, with parsed evidence under `Docs/BlueprintContractInventory/Phase3-InventoryFoundation/`. Comparing the exact same tested assets against checkpoint 5:

- Failed Blueprints: 12 -> 10 (2 repaired)
- Fatal issues: 431 -> 170 (261 removed)
- Warnings attached to failed Blueprints: 87 -> 19 (68 removed)
- Newly compiling assets: `WBP_InvItemTooltip` and `WBP_VendorItemObject`
- `WBP_NPCVendorShop`: 81 -> 16 fatal issues
- `WBP_NPCVendorShopSell`: 95 -> 12
- `WBP_PlayerInventory`: 57 -> 9
- `WBP_ItemObject`: 33 -> 9

The remaining errors are now cleanly separated into downstream contracts: character/controller actions (`UseItem`, `DropItem`), equipment item subclasses and capture functions, crafting recipe objects/components, and a few widget-owned references. No asset was resaved to obtain this reduction.

Current failing Blueprint clusters after parent restoration:

- Inventory/vendor/crafting UI: highest concentration, including missing item/inventory API contracts.
- GAS attribute/spell UI: missing widget-controller properties, delegates, functions, and attribute-info structs.
- Quest UI: missing quest actor/objective/reward contracts.
- World widgets: missing widget-controller and effect-component contracts.
- Animation: four legacy animation Blueprints also have missing skeletons and character/controller contracts.

Map-load findings:

- Main Menu loaded into play without map-specific load failures.
- Tutorial loaded into play and initialized World Partition.
- Tutorial reported a stale HLOD settings import from the editor-only `WorldPartitionHLODUtilities` script package; the map still completed loading.
- The project omitted the Gameplay Tag config used by its Foley assets. The 12 serialized `Foley.Event.*` tags were restored in `DefaultGameplayTags.ini`; a repeat Tutorial load confirmed the invalid-tag warnings are gone.
- `BP_PlayerCharacter` contains a removed `DebugSessionUniqueIdentifier` input on a Motion Match node. It remains a warning until the Blueprint can be safely refreshed and saved after native contracts are restored.

## Focused acceptance checks

### GAS and spells

- Ability actor info initializes after spawn, possession, replication, and respawn.
- Attributes initialize and update all bound UI.
- Abilities activate through intended inputs and respect costs, cooldowns, tags, and targets.
- Damage, healing, death, experience, and rewards apply once and replicate where intended.
- Spell unlock, equip, swap, cast, impact, VFX, animation, sound, and cooldown display match the baseline.

### Quests

- Quest offer, acceptance, objectives, notification, log entry, marker, completion, and rewards work in sequence.
- Collection, interaction, location, and combat objectives update once per qualifying event.
- Quest state survives UI reconstruction, map travel, save, and reload.

### Inventory and equipment

- Pickup, stack, split, move, equip, unequip, use, drop, buy, sell, and craft operations preserve item counts and currency.
- Invalid drag/drop targets and full-inventory cases fail safely.
- Equipment modifies the correct stats and restores them when removed.
- Inventory and equipment survive death, travel, save, and reload as designed.

### UI and maps

- Main Menu reaches the intended gameplay map.
- HUD, attributes, spell menu, inventory, equipment, crafting, vendors, quest log, pause, and world widgets open and close without duplicate bindings.
- Keyboard/mouse and gamepad focus, navigation, input modes, DPI scaling, and resolution behavior remain usable.
- `MainMenu` and `Tutorial` load without missing classes, external actors, materials, animations, or navigation data.

## Modernization policy

Potential improvements are evaluated only after parity. Candidate work includes data-driven quest/class/spell/item definitions, centralized Gameplay Tags, UI view-model boundaries, versioned saves, stable item identifiers, soft references for optional content, and automated functional smoke tests. Each improvement requires a separate branch and regression evidence.
