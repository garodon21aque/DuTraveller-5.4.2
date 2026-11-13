"""Inspect actual UI graphs and player defaults in the editor without saving assets."""
import json
from pathlib import Path
import unreal

report = {"assets": [], "defaults": {}}
try:
    for path in (
        "/Game/UI/HUD/WBP_HUD", "/Game/UI/InventorySystem/WBP_PlayerInventory",
        "/Game/UI/CraftingSystem/WBP_CraftingMenu", "/Game/UI/QuestSystem/WBP_QuestLog",
        "/Game/UI/Attribute/WBP_AttributeMenu", "/Game/UI/Attribute/WBP_TextValueButtonROw",
        "/Game/UI/Attribute/WBP_AttributesPointsRow", "/Game/UI/InventorySystem/WBP_DropWidget",
        "/Game/UI/SpellMenu/WBP_SpellMenu", "/Game/UI/HUD/WBP_Talents",
        "/Game/UI/HUD/WBP_Minimap", "/Game/Blueprints/Characters/BP_PlayerCharacter",
        "/Game/Blueprints/Framework/BP_PlayerController", "/Game/Blueprints/Framework/BP_PlayerState",
    ):
        asset = unreal.load_asset(path)
        if not asset:
            report["assets"].append({"asset": path, "missing": True})
            continue
        unreal.BlueprintEditorLibrary.compile_blueprint(asset)
        parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(asset)
        report["assets"].append({"asset": path, "status": str(asset.get_editor_property("status")),
                                  "parent": parent.get_path_name() if parent else None,
                                  "members": [str(n) for n in unreal.BlueprintEditorLibrary.list_member_variable_names(asset) if "/" not in str(n)]})
        if path.startswith("/Game/UI/"):
            unreal.DTStructRepairLibrary.dump_blueprint_graph(path)
    state_class = unreal.load_class(None, "/Game/Blueprints/Framework/BP_PlayerState.BP_PlayerState_C")
    state = unreal.get_default_object(state_class)
    for prop in ("starting_attribute_points", "starting_skill_points", "attribute_points", "spell_points"):
        report["defaults"][prop] = state.get_editor_property(prop)
    skills = state.get_editor_property("skills")
    report["defaults"]["available_abilities"] = [a.get_path_name() for a in skills.get_editor_property("available_abilities")]
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    report["animations"] = []
    for root in ("/Game/Art/3D/Animations", "/Game/Blueprints/Animations"):
        for data in registry.get_assets_by_path(root, True):
            if str(data.asset_class_path.asset_name) not in ("AnimMontage", "AnimBlueprint"):
                continue
            try:
                asset = data.get_asset()
            except Exception as error:
                report["animations"].append({"asset": str(data.package_name), "load_error": str(error)})
                continue
            prop = "target_skeleton" if isinstance(asset, unreal.AnimBlueprint) else "skeleton"
            skeleton = asset.get_editor_property(prop)
            report["animations"].append({"asset": asset.get_path_name(), "skeleton": skeleton.get_path_name() if skeleton else None})
finally:
    output = Path(unreal.Paths.project_saved_dir()) / "Automation" / "UnifiedUIInspection.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("UNIFIED_UI_INSPECTION " + json.dumps(report))
    unreal.SystemLibrary.quit_editor()
