"""Create consumed catalogue tables and bridge current quest data into the empty legacy table.

Existing populated tables are preserved. Current definitions are reconstructed
content; this script does not claim to recover missing historical quest rows.
"""
import json
from pathlib import Path
import re
import traceback
import unreal

report = {"passed": False, "tables": [], "provenance": "Current executable definitions; not recovered historical balance or quest content",
          "original_quest_table": "GitHub master contents API returned 404 for Content/Blueprints/Quest/QuestData.uasset"}
tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.DataTableFunctionLibrary

def save(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
        raise RuntimeError("Save failed: " + asset.get_path_name())

def table(path, struct_path, rows, migrate_legacy_schema=False):
    asset = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    struct = unreal.load_object(None, struct_path)
    if not struct:
        raise RuntimeError("Missing row schema: " + struct_path)
    if not asset:
        parent, name = path.rsplit("/", 1)
        factory = unreal.DataTableFactory()
        factory.set_editor_property("struct", struct)
        asset = tools.create_asset(name, parent, unreal.DataTable, factory)
    if not asset:
        raise RuntimeError("Table could not be created: " + path)
    migrated = False
    if library.get_data_table_row_struct(asset) != struct:
        current = library.get_data_table_row_struct(asset)
        names = {str(value) for value in library.get_data_table_row_names(asset)}
        if not (migrate_legacy_schema and current.get_path_name() == "/Script/DuTraveller.QuestDetails"
                and names <= {row["Name"] for row in rows}):
            raise RuntimeError("Table has an unexpected row schema: " + path)
        before = library.export_data_table_to_json_string(asset)
        previous_export = Path(unreal.Paths.project_saved_dir()) / "Automation/QuestData-BeforeLegacySchema.json"
        if names and (not previous_export.exists() or json.loads(previous_export.read_text(encoding="utf-8")) != json.loads(before)):
            raise RuntimeError("Refusing to replace populated native quest data without a matching reconstruction checkpoint")
        previous_export.write_text(before, encoding="utf-8")
        if not library.fill_data_table_from_json_string(asset, json.dumps(rows), struct):
            raise RuntimeError("Legacy row-schema migration failed")
        if library.get_data_table_row_struct(asset) != struct:
            raise RuntimeError("Legacy row schema did not change")
        migrated = True
        save(asset)
    names = library.get_data_table_row_names(asset)
    preserved = bool(names)
    if not names:
        if not library.fill_data_table_from_json_string(asset, json.dumps(rows), struct):
            raise RuntimeError("Data import failed: " + path)
        save(asset)
    names = [str(value) for value in library.get_data_table_row_names(asset)]
    if not names:
        raise RuntimeError("Refusing an empty gameplay table: " + path)
    report["tables"].append({"asset": asset.get_path_name(), "row_struct": struct_path, "rows": names,
                             "preserved_existing_rows": preserved and not migrated, "migrated_reconstructed_schema": migrated})
    return asset

try:
    state_path = "/Game/Blueprints/Framework/BP_PlayerState"
    state_bp = unreal.load_asset(state_path)
    state = unreal.get_default_object(state_bp.generated_class())
    skills = state.get_editor_property("skills")
    definitions = list(skills.get_available_definitions())
    if not definitions:
        raise RuntimeError("No executable player skills are available to catalogue")
    skill_rows = [{"Name": str(asset.get_editor_property("ability_id")), "Definition": asset.get_path_name()} for asset in definitions]
    skill_table = table("/Game/Data/Catalogues/DT_PlayerAbilities", "/Script/DuTraveller.RPGAbilityCatalogueRow", skill_rows)
    skills.set_editor_property("ability_catalogue", skill_table)
    available = list(skills.get_available_definitions())
    if not all(asset in available for asset in definitions):
        raise RuntimeError("The skill table did not resolve the existing executable definitions")
    report["resolved_skills"] = [str(asset.get_editor_property("ability_id")) for asset in available]

    group = unreal.load_asset("/Game/Data/Quests/DA_QuestGroup_LocationDiscovery")
    choices = list(group.get_quest_entries())
    if not choices:
        raise RuntimeError("No configured location quests are available to catalogue")
    quest_rows = [{"Name": str(entry.get_editor_property("definition").get_editor_property("quest_id")),
                   "Definition": entry.get_editor_property("definition").get_path_name(),
                   "Weight": entry.get_editor_property("weight")} for entry in choices]
    quest_table = table("/Game/Data/Catalogues/DT_LocationQuests", "/Script/DuTraveller.RPGQuestCatalogueRow", quest_rows)
    group.set_editor_property("quest_catalogue", quest_table)
    resolved = list(group.get_quest_entries())
    if len(resolved) < len(choices):
        raise RuntimeError("The quest table lost configured entries")
    report["resolved_quests"] = [str(entry.get_editor_property("definition").get_editor_property("quest_id")) for entry in resolved]

    # The retained legacy table is a presentation bridge. Gameplay and rolled
    # rewards continue to use the server-owned dynamic definition and record.
    legacy_rows = []
    for choice in resolved:
        definition = choice.get_editor_property("definition")
        if definition.get_editor_property("audience") != unreal.RPGQuestAudience.PERSONAL:
            raise RuntimeError("Legacy category mapping must be authored explicitly for non-personal quests")
        title = str(definition.get_editor_property("title"))
        description = str(definition.get_editor_property("description"))
        objectives = []
        for index, objective in enumerate(definition.get_editor_property("objectives")):
            objective_name = re.sub(r"(?<=[a-z])(?=[A-Z])", " ", str(objective.get_editor_property("objective_id")))
            event = str(objective.get_editor_property("event"))
            types = {"Interacted": "Interact", "Collected": "Collect", "Killed": "Kill", "Talked": "Talk", "Entered": "Location"}
            if event not in types:
                raise RuntimeError("Legacy objective enum cannot represent event " + event)
            objectives.append(dict(ObjectiveID=str(objective.get_editor_property("objective_id")), ObjectiveName=objective_name,
                                   ObjectiveDescription=description, Quantity=objective.get_editor_property("required_count"),
                                   TrackingDescription=objective_name, ObjectiveType=types[event], BonusXP=0, BonusRenown=0,
                                   isOptional=objective.get_editor_property("optional")))
        currencies, items = {}, []
        for reward in definition.get_editor_property("guaranteed_rewards"):
            item_class = reward.get_editor_property("item_class")
            quantity = reward.get_editor_property("quantity")
            if isinstance(unreal.get_default_object(item_class), unreal.Currency_ItemObject):
                key = item_class.get_path_name()
                currencies[key] = currencies.get(key, 0) + quantity
            else:
                if len(items) + quantity > 128:
                    raise RuntimeError("Legacy item reward array cannot represent this quantity safely")
                items.extend([item_class.get_path_name()] * quantity)
        experience = float(definition.get_editor_property("experience_reward"))
        if len(currencies) > 1 or not experience.is_integer() or experience > 2147483647 or sum(currencies.values()) > 2147483647:
            raise RuntimeError("Legacy quest schema cannot represent these currencies or XP without loss")
        stage = dict(StageName=title, StageDescription=description, LogDescription=description,
                     XPReward=0, CurrencyRewards=0, Renown=0, Objectives=objectives, ItemRewards=[])
        legacy_rows.append(dict(Name=str(definition.get_editor_property("quest_id")), QuestName=title,
                                LogDescription=description, QuestTypes="Main", Stages=[stage], ItemRewards=items,
                                CurrencyRewards=sum(currencies.values()), XPReward=int(experience)))
    table("/Game/Blueprints/Quest/QuestData", "/Game/Blueprints/Quest/FQuestDetails.FQuestDetails", legacy_rows, True)

    save(group)
    if not unreal.DTStructRepairLibrary.force_compile_and_save(state_path):
        raise RuntimeError("PlayerState Blueprint compile/save guard refused the table assignment")
    player_bp = unreal.load_asset("/Game/Blueprints/Characters/BP_PlayerCharacter")
    player = unreal.get_default_object(player_bp.generated_class())
    crafting = player.get_editor_property("crafting_comp")
    report["inline_crafting_recipes"] = len(crafting.get_recipes())
    recipe_table = crafting.get_editor_property("recipe_catalogue")
    report["crafting_recipe_table"] = recipe_table.get_path_name() if recipe_table else None
    report["catalogued_crafting_recipe_rows"] = len(library.get_data_table_row_names(recipe_table)) if recipe_table else 0
    evidence = json.loads((Path(unreal.Paths.project_dir()) / "Docs/ClassSkillTreeEvidence.json").read_text(encoding="utf-8"))
    evidenced = {tag for tree in evidence for tag in tree.get("SerializedAbilityTags", [])}
    report["evidenced_skills_without_executable_definition"] = sorted(evidenced - set(report["resolved_skills"]))
    for row in report["tables"]:
        asset = unreal.load_asset(row["asset"])
        export = library.export_data_table_to_json_string(asset)
        name = row["asset"].split(".")[-1]
        (Path(unreal.Paths.project_saved_dir()) / "Automation" / (name + "-DataExport.json")).write_text(export, encoding="utf-8")
    report["passed"] = True
except Exception:
    report["exception"] = traceback.format_exc()
    raise
finally:
    output = Path(unreal.Paths.project_saved_dir()) / "Automation/GameplayDataTables.json"
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("DT_GAMEPLAY_TABLES " + json.dumps(report))
    unreal.SystemLibrary.quit_editor()
