"""Create or promote the production spawn/quest base assets without touching legacy game content."""
import json
from pathlib import Path
import unreal

BPROOT = "/Game/Blueprints/Gameplay/Spawning"
DATAROOT = "/Game/Data/Quests"
tools = unreal.AssetToolsHelpers.get_asset_tools()
created = []
promoted = []


def promote(old_path, new_path):
    if unreal.EditorAssetLibrary.does_asset_exist(new_path):
        return
    if unreal.EditorAssetLibrary.does_asset_exist(old_path):
        if not unreal.EditorAssetLibrary.rename_asset(old_path, new_path):
            raise RuntimeError("Could not promote " + old_path + " to " + new_path)
        promoted.append({"from": old_path, "to": new_path})


def data(name, cls, values):
    path = DATAROOT + "/" + name
    existing = unreal.load_asset(path)
    if existing:
        if not isinstance(existing, cls):
            raise RuntimeError("Unexpected existing asset: " + path)
        asset = existing
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", cls)
        asset = tools.create_asset(name, DATAROOT, cls, factory)
        if not asset:
            raise RuntimeError("Could not create " + path)
        created.append(path)
    for key, value in values.items():
        asset.set_editor_property(key, value)
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
        raise RuntimeError("Could not save " + path)
    return asset


def blueprint(name, parent):
    path = BPROOT + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.EditorAssetLibrary.load_blueprint_class(path)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    asset = tools.create_asset(name, BPROOT, unreal.Blueprint, factory)
    if not asset or not unreal.DTStructRepairLibrary.force_compile_and_save(asset.get_path_name()):
        raise RuntimeError("Blueprint compile/save failed: " + path)
    created.append(path)
    return unreal.EditorAssetLibrary.load_blueprint_class(path)


try:
    unreal.EditorAssetLibrary.make_directory(BPROOT)
    unreal.EditorAssetLibrary.make_directory(DATAROOT)
    migrations = {
        "/Game/RestorationExamples/SpawningQuests/BP_RandomSpawnArea": BPROOT + "/BP_MasterRandomSpawnArea",
        "/Game/RestorationExamples/SpawningQuests/BP_LocationQuestSpawner": BPROOT + "/BP_MasterLocationQuestSpawner",
        "/Game/RestorationExamples/SpawningQuests/BP_ExampleQuestTarget": BPROOT + "/BP_MasterQuestTarget",
        "/Game/RestorationExamples/SpawningQuests/DA_InspectRuins": DATAROOT + "/DA_Quest_InspectRuins",
        "/Game/RestorationExamples/SpawningQuests/DA_InspectSupplies": DATAROOT + "/DA_Quest_InspectSupplies",
        "/Game/RestorationExamples/SpawningQuests/DA_ExampleQuestGroup": DATAROOT + "/DA_QuestGroup_LocationDiscovery",
    }
    for old_path, new_path in migrations.items():
        promote(old_path, new_path)
    objective = unreal.RPGDynamicObjective()
    objective.set_editor_property("objective_id", "Inspect")
    objective.set_editor_property("event", "Interacted")
    objective.set_editor_property("required_count", 1)
    currency = unreal.RPGInventoryAmount()
    currency.set_editor_property("item_class", unreal.Currency_ItemObject)
    currency.set_editor_property("quantity", 10)
    rewards = []
    for cls, weight in [(unreal.Food_ItemObject, 3), (unreal.Potion_ItemObject, 1)]:
        reward = unreal.RPGRandomRewardEntry()
        reward.set_editor_property("item_class", cls)
        reward.set_editor_property("weight", weight)
        rewards.append(reward)
    definitions = []
    for name, quest_id, title in [("DA_Quest_InspectRuins", "Quest.Location.InspectRuins", "Inspect the ruins"),
                                  ("DA_Quest_InspectSupplies", "Quest.Location.InspectSupplies", "Inspect the supply cache")]:
        definitions.append(data(name, unreal.RPGRandomQuestDefinition, dict(
            quest_id=quest_id, title=title, description="Interact with the quest-linked objective in the ruins.",
            allowed_locations=["Location.Ruins"], objectives=[objective], guaranteed_rewards=[currency],
            random_rewards=rewards, experience_reward=25.0)))
    choices = []
    for definition, weight in zip(definitions, [3, 1]):
        entry = unreal.RPGWeightedQuest()
        entry.set_editor_property("definition", definition)
        entry.set_editor_property("weight", weight)
        choices.append(entry)
    group = data("DA_QuestGroup_LocationDiscovery", unreal.RPGQuestGroupDefinition,
                 dict(group_id="QuestGroup.LocationDiscovery", quests=choices, maximum_active_per_player=1))
    blueprint("BP_MasterRandomSpawnArea", unreal.RPGSpawnArea)
    blueprint("BP_MasterLocationQuestSpawner", unreal.RPGRandomQuestSpawner)
    # Concrete pickup class provides a valid scene root; designers choose their
    # own character/pickup/encounter Blueprint classes for production pools.
    blueprint("BP_MasterQuestTarget", unreal.RPGPickupItem)
    for asset in definitions + [group]:
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    report = dict(created=created, promoted=promoted, blueprint_root=BPROOT, data_root=DATAROOT,
                  note="Production base assets; balance values remain configurable reconstruction defaults")
    report_path = Path(unreal.Paths.project_saved_dir()) / "Automation" / "ProductionSpawnQuestAssets.json"
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("PRODUCTION_SPAWN_QUEST_ASSETS " + json.dumps(report))
except Exception:
    import traceback
    unreal.log_error("PRODUCTION_SPAWN_QUEST_ASSETS " + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
