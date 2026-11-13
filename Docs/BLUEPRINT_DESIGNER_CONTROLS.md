# DuTraveller designer controls

The active player framework inherits the shared RPG gameplay layer. The DT
Motion Matching and traversal Blueprint remains the locomotion owner.

## Player state

Open `/Game/Blueprints/Framework/BP_PlayerState` and its Class Defaults:

- **DT / Starting Progression**: Starting Attribute Points (5), Starting Skill
  Points (3), and Grant Starting Progression. The server grants these once per
  new player state; copied travel state retains its existing balance.
- Select the **Skills** component: **Ability Catalogue** selects
  `/Game/Data/Catalogues/DT_PlayerAbilities`. Rows reference executable skill
  definitions and their names must match each definition's Ability ID. A
  configured table controls availability; Available Abilities is the fallback
  when the table is unset. Starter Hotbar controls startup learning/equipment.
- Skill definitions in `/Game/Data/Abilities/Player` expose damage by rank,
  resource costs, range, cooldown, prerequisites, and presentation metadata.
  These starter definitions are newly authored defaults, not recovered balance.

## Player controller and UI

Open `/Game/Blueprints/Framework/BP_PlayerController`:

- **DT / UI / Panel Classes** selects menu widgets. Panel IDs are Inventory,
  Crafting, Skills, Talents, Attributes, Quests, Party, Vendor, Guild, Pause,
  and Settings.
- Use `Open UI Panel`, `Toggle UI Panel`, and `Close UI Panels` from Blueprint
  buttons. Use `Register UI Panel` to adopt a pre-existing widget instance.
- Use the player's `UI Root` to attach persistent HUD content and notifications.
- Panel widgets must receive the owning player when created. `RPG_UserWidget`
  resolves inventory, crafting, and controller context before Blueprint events.
- `WBP_PlayerInventory` inherits `DTPlayerInventoryWidget`: **DT / Inventory
  Preview** selects the material, anchors, offsets and visibility of the preview.
  The existing `MI_PlayerPreview` material consumes the front camera's target.
- `WBP_DropWidget` inherits `DTDropQuantityWidget`. Designers can disable
  **Build Default Quantity Controls** and wire their own buttons to Increase
  Quantity, Decrease Quantity, Confirm Quantity and Cancel Quantity.
- `Request Plant Farm`, `Request Harvest Farm`, and `Request Place Building`
  are owning-client entry points. The server still checks permissions, costs,
  distance, state, and the controller's **Buildable Definitions** catalogue.

Default keyboard routes: I inventory, C crafting, P party, K attributes,
J skills, T talents, L quests, G guild, Escape close/pause.

Combat hotbar defaults are left/right mouse for slots 0/1 and number keys 1–0
for slots 2–11. The controller's `Activate Hotbar Slot` is also callable from
Blueprint buttons. `Enable Combat Hotbar Input` disables these requests, and
`Resolve Combat Target` is a Blueprint-overridable function. Its default traces
from the camera; the server checks target range, line of sight, costs and cooldown.
Opening a panel suppresses combat hotbar requests. Physical input conflicts
with traversal and gamepad bindings still require an interactive Editor check.

## Character cameras and animation

Open `/Game/Blueprints/Characters/BP_PlayerCharacter`:

- **DT / Cameras** exposes preview and face offsets, minimap height and width,
  render targets, and minimap enablement. `Initialize Player Captures` refreshes
  these settings for the locally controlled player.
- **DT_AnimationRouter** exposes Combat Animations keyed by ability ID and a
  Hit Reaction montage. Each entry selects a montage, section and play rate.
- Assign only montages retargeted to the active character skeleton. The router
  rejects mismatched skeletons and defers to an external/traversal montage.
- Use **DT Animation Event** notifies and **DT Combo Window** notify states in
  authored combat montages. `On Animation Event` supports Blueprint cosmetics;
  server events also enter GAS on the same avatar.
- Accepted nonlethal damage requests the router's Hit Reaction on relevant
  clients; the router retains the same traversal and skeleton guards.
- The component does not replace `ABP_SandboxCharacter`, Pose Search databases,
  traversal montages, or root-motion configuration. Combat-slot placement and
  missing source skeletons require explicit animation asset validation.

The UE4 mannequin source dependency was recovered from Epic's installed
AnimToTexture content into `/Game/Characters/Mannequin_UE4`, including its mesh,
68-bone reference skeleton, materials and textures. The project copies use
project references and do not require that plugin to be enabled.

The staged rigs and retargeter are in `/Game/DTIntegration/Animation`:
`IK_DT_LegacyUE4`, `IK_DT_UEFN`, and `RTG_DT_LegacyUE4_UEFN`. The `Combat`
subfolder contains a retargeted slash montage and sequence with the `_UEFN`
suffix. A headless PIE check starts that montage through the shared router.
It remains unassigned in the live character's Combat Animations map until
visual pose, blending, slot placement, root motion and combo timing checks
are completed. The original montage was not modified.

## Farming and building assets

`RPGBuildingDefinition` Data Assets expose class, construction costs, maximum
level and interaction range. Farm Blueprint defaults expose seed and harvest
item classes, harvest quantity and growth duration. `Farm State Changed` is
the Blueprint presentation hook for empty/growing/ready crop appearance.

`WBP_QuestLog` inherits `DTQuestJournalWidget`. Its native bridge appends a
dedicated journal container to the retained `MainSb` scroll box. It displays
PlayerState-owned quests, progress, completion and nearby published offers;
acceptance and reward buttons use the existing server-validated requests.
**DT / Quests** exposes the host widget name, default rendering toggle and a
Blueprint-replaceable entry widget class. Custom rows can read Quest Title,
Quest Summary and Action Label and call `Perform Quest Action`. Custom journals
can use `Quest Journal Changed` and `Refresh Journal`.

Legacy quest tables and dynamic quest definitions remain different schemas.
The journal bridge displays the current dynamic data; it does not reconstruct
missing historical quest content or silently convert the old tables.

## Gameplay data tables

`DT_PlayerAbilities` is consumed by learning, hotbar startup and the skill UI.
Its rows point to the skill Data Assets, which own balance and presentation.
The same authoritative catalogue also validates client learning requests.

`/Game/Data/Catalogues/DT_LocationQuests` is assigned to
`DA_QuestGroup_LocationDiscovery`. Each row selects a quest definition and its
selection weight. Table entries override inline entries with the same Quest ID.
The quest spawner consumes the resolved entries when publishing offers.

`/Game/Blueprints/Quest/QuestData` now uses the retained Blueprint
`FQuestDetails` row struct, including stages and `FObjectiveDetails` entries.
Its two rows mirror the current location quests for legacy widget compatibility.
Those rows are reconstructed from current definitions; the connected historical
GitHub repository did not contain the original QuestData asset. Dynamic records
remain authoritative for progress and rolled rewards. The legacy schema's Main
category is a compatibility grouping for these two personal quests.

`/Game/Data/Catalogues/DT_CraftingRecipes` selects recipe Blueprint classes.
The character's Crafting Comp loads these on the server and replicates the
recipe instances. The starter row points to
`/Game/Data/Recipes/BP_DT_HealthPotionRecipe`: two
`/Game/Data/Items/BP_DT_WildHerb` items and one existing bronze coin produce
one `/Game/Data/Items/BP_DT_HealthPotion`. The potion's Class Defaults select
the GAS restoration effect and its Health magnitude (25). These costs and
values are new editable defaults. No ingredients are granted automatically.
Restart play after changing the recipe catalogue or recipe defaults.

The table integration report under `Saved/Automation/GameplayDataTables.json`
lists evidenced skill tags that still lack executable definitions. Unsupported
skills are not advertised as functional through placeholder rows.
