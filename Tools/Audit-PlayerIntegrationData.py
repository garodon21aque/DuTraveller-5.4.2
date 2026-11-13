"""Read-only standalone Editor audit; does not save project assets."""
import json
from pathlib import Path
import traceback
import unreal

report = {"saved_assets": False}
try:
    table = unreal.load_asset("/Game/Blueprints/Quest/QuestData")
    if table:
        report["legacy_quest_table"] = table.get_path_name()
        report["legacy_quest_rows"] = [str(value) for value in unreal.DataTableFunctionLibrary.get_data_table_row_names(table)]
        report["legacy_quest_export"] = unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table)
    else:
        report["legacy_quest_table_missing"] = True
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    report["dynamic_quest_data"] = [str(data.package_name) for data in registry.get_assets_by_path("/Game/Data/Quests", True)]
    state = unreal.get_default_object(unreal.load_class(None, "/Game/Blueprints/Framework/BP_PlayerState.BP_PlayerState_C"))
    skills = state.get_editor_property("skills")
    report["player_abilities"] = [{"asset": definition.get_path_name(),
                                   "ability_id": str(definition.get_editor_property("ability_id")),
                                   "icon": str(definition.get_editor_property("presentation").get_editor_property("icon"))}
                                  for definition in skills.get_available_definitions()]
    catalogue = skills.get_editor_property("ability_catalogue")
    report["player_ability_table"] = catalogue.get_path_name() if catalogue else None
    unreal.DTStructRepairLibrary.dump_blueprint_graph("/Game/UI/CraftingSystem/WBP_ItemBpTooltip")
    unreal.DTStructRepairLibrary.dump_blueprint_graph("/Game/UI/CraftingSystem/WBP_CraftingMenu")
    report["all_tables"] = []
    options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
    for data in registry.get_assets_by_path("/Game", True):
        if str(data.asset_class_path.asset_name) == "DataTable":
            asset = data.get_asset()
            report["all_tables"].append({"asset": asset.get_path_name(),
                "rows": [str(name) for name in unreal.DataTableFunctionLibrary.get_data_table_row_names(asset)],
                "referencers": [str(name) for name in registry.get_referencers(data.package_name, options)]})
except Exception:
    report["exception"] = traceback.format_exc()
finally:
    output = Path(unreal.Paths.project_saved_dir()) / "Automation" / "PlayerIntegrationData.json"
    output.write_text(json.dumps(report, indent=2, default=str), encoding="utf-8")
    unreal.log("DT_PLAYER_DATA_AUDIT " + str(output))
