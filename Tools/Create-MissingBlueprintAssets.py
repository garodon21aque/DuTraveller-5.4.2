"""Recreates Blueprint assets that were referenced by serialized graphs but
never committed to the repository.

Each entry preserves the exact original /Game package path and parents the new
Blueprint to the reconstructed native contract in /Script/DuTraveller (or the
nearest evidenced parent). The script refuses to replace existing assets.

Run with the project editor's -ExecutePythonScript or via the in-editor Python
console after the DuTraveller module has been built.
"""

import unreal


# (package path, asset name, parent class script path)
BLUEPRINT_CLASSES = (
    # Player framework
    ("/Game/Blueprints/Player", "BP_PlayerCharacter", "/Script/DuTraveller.RPGCharacter"),
    ("/Game/Blueprints/Player", "BP_RPGCharacter", "/Script/DuTraveller.RPGCharacter"),
    ("/Game/Blueprints/Player", "BP_RPGPlayerController", "/Script/DuTraveller.RPGPlayerController"),
    ("/Game/Blueprints/Framework", "BP_RPGPlayerState", "/Script/DuTraveller.RPGPlayerState"),
    # AI / NPC
    ("/Game/Blueprints/AI", "BP_RPGAICharacter", "/Script/DuTraveller.RPGAICharacter"),
    ("/Game/Blueprints/AI", "BP_AIRangedCharacter", "/Script/DuTraveller.RPGAICharacter"),
    ("/Game/Blueprints/AI", "BP_RPGAIController", "/Script/DuTraveller.RPGAIController"),
    ("/Game/Blueprints/AI/NPC", "BP_NPC_Trainer", "/Script/DuTraveller.RPGAICharacter"),
    ("/Game/Blueprints/AI/NPC", "BP_AIRPGActionComponent", "/Script/DuTraveller.RPGActionComponent"),
    # Actions
    ("/Game/Blueprints/Actions/Melee", "BP_Action_Swing", "/Script/DuTraveller.RPGAction"),
    ("/Game/Blueprints/Actions/Melee", "BP_RPGAction_MeleeAttack", "/Script/DuTraveller.RPGAction"),
    # Quests
    ("/Game/Blueprints/Quest", "BP_QuestGiver", "/Script/DuTraveller.RPGQuestGiver"),
    ("/Game/Blueprints/Quest", "BP_QuestLog", "/Script/DuTraveller.RPGQuestComponent"),
    # Pickups / items
    ("/Game/Blueprints/Actors/Interactables/Items", "BP_MasterPickupItem", "/Script/DuTraveller.RPGPickupItem"),
    ("/Game/Blueprints/InventorySystem/Items/MoneyItem", "BP_BronzeItem", "/Script/DuTraveller.ItemObject"),
    ("/Game/Blueprints/InventorySystem/Items/MoneyItem", "BP_GoldItem", "/Script/DuTraveller.ItemObject"),
    # Village buildings
    ("/Game/Blueprints/Buildings/Village", "BP_Farm", "/Script/DuTraveller.RPGBuilding"),
    ("/Game/Blueprints/Buildings/Village", "BP_Tavern", "/Script/DuTraveller.RPGBuilding"),
    ("/Game/Blueprints/Buildings/Village", "BP_TownHall", "/Script/DuTraveller.RPGBuilding"),
    # Animation notify
    ("/Game/Blueprints/Notify", "BP_DealDamageNotify", "/Script/Engine.AnimNotify"),
)

# Created after the parents above so it can derive from BP_MasterPickupItem.
CHILD_BLUEPRINTS = (
    ("/Game/Blueprints/Actors/Interactables/Items", "BP_Pickup",
     "/Game/Blueprints/Actors/Interactables/Items/BP_MasterPickupItem.BP_MasterPickupItem_C"),
)

DATA_TABLES = (
    ("/Game/Blueprints/Quest", "QuestData", "/Script/DuTraveller.QuestDetails"),
)


def create_blueprint(asset_tools, package_path, asset_name, parent_path):
    object_path = "{0}/{1}.{1}".format(package_path, asset_name)
    if unreal.EditorAssetLibrary.does_asset_exist(object_path):
        unreal.log("Blueprint already exists: {0}".format(object_path))
        return

    parent_class = unreal.load_class(None, parent_path)
    if not parent_class:
        raise RuntimeError("Unable to load parent class {0}".format(parent_path))

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    blueprint = asset_tools.create_asset(asset_name, package_path, unreal.Blueprint, factory)
    if not blueprint:
        raise RuntimeError("Unable to create {0}".format(object_path))
    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False):
        raise RuntimeError("Unable to save {0}".format(object_path))
    unreal.log("Created Blueprint: {0} -> {1}".format(object_path, parent_path))


def create_data_table(asset_tools, package_path, asset_name, struct_path):
    object_path = "{0}/{1}.{1}".format(package_path, asset_name)
    if unreal.EditorAssetLibrary.does_asset_exist(object_path):
        unreal.log("Data table already exists: {0}".format(object_path))
        return

    row_struct = unreal.load_object(None, struct_path) or unreal.find_object(None, struct_path)
    if not row_struct:
        raise RuntimeError("Unable to load row struct {0}".format(struct_path))

    factory = unreal.DataTableFactory()
    factory.set_editor_property("struct", row_struct)
    table = asset_tools.create_asset(asset_name, package_path, unreal.DataTable, factory)
    if not table:
        raise RuntimeError("Unable to create {0}".format(object_path))
    if not unreal.EditorAssetLibrary.save_loaded_asset(table, only_if_is_dirty=False):
        raise RuntimeError("Unable to save {0}".format(object_path))
    unreal.log("Created DataTable: {0} -> {1}".format(object_path, struct_path))


def main():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

    for package_path, asset_name, parent_path in BLUEPRINT_CLASSES:
        create_blueprint(asset_tools, package_path, asset_name, parent_path)
    for package_path, asset_name, parent_path in CHILD_BLUEPRINTS:
        create_blueprint(asset_tools, package_path, asset_name, parent_path)
    for package_path, asset_name, struct_path in DATA_TABLES:
        create_data_table(asset_tools, package_path, asset_name, struct_path)


try:
    main()
except Exception as exc:  # noqa: BLE001 - report then still release the editor process
    unreal.log_error("Create-MissingBlueprintAssets failed: {0}".format(exc))
    raise
finally:
    unreal.SystemLibrary.quit_editor()
