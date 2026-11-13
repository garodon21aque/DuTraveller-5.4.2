"""Read/compile validation for the DT Motion Matching + RPG/GAS class bridge. Never saves assets."""
import json
from pathlib import Path
import traceback
import unreal

TARGETS = [
    "/Game/Blueprints/Characters/BP_PlayerCharacter",
    "/Game/Blueprints/Framework/BP_PlayerController",
    "/Game/Blueprints/Framework/BP_PlayerState",
    "/Game/Blueprints/Framework/DT_GameModeBase",
    "/Game/Blueprints/Framework/BP_RPGGameModeBase",
]

report = {"passed": False, "assets": []}
try:
    for path in TARGETS:
        asset = unreal.load_asset(path)
        if not isinstance(asset, unreal.Blueprint):
            raise RuntimeError(f"Missing Blueprint: {path}")
        unreal.BlueprintEditorLibrary.compile_blueprint(asset)
        parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(asset)
        generated = asset.generated_class()
        default = unreal.get_default_object(generated) if generated else None
        row = {
            "asset": path,
            "status": str(asset.get_editor_property("status")),
            "parent": parent.get_path_name() if parent else None,
            "generated_class": generated.get_path_name() if generated else None,
        }
        if isinstance(default, unreal.Actor):
            row["components"] = sorted(
                component.get_class().get_path_name()
                for component in default.get_components_by_class(unreal.ActorComponent)
            )
        if isinstance(default, unreal.GameModeBase):
            for prop in ("default_pawn_class", "player_controller_class", "player_state_class", "hud_class"):
                value = default.get_editor_property(prop)
                row[prop] = value.get_path_name() if value else None
        report["assets"].append(row)
        if "ERROR" in row["status"].upper():
            raise RuntimeError(f"Blueprint compile failed: {path}: {row['status']}")

    character = unreal.get_default_object(unreal.load_class(None,
        "/Game/Blueprints/Characters/BP_PlayerCharacter.BP_PlayerCharacter_C"))
    component_names = {
        component.get_class().get_name()
        for component in character.get_components_by_class(unreal.ActorComponent)
    }
    required = {
        "InventoryComponent", "RPGAttributeComponent", "RPGActionComponent",
        "RPGCraftingComponent", "RPGQuestComponent", "AbilitySystemComponent",
    }
    report["modern_character_required_components"] = sorted(required)
    report["modern_character_component_names"] = sorted(component_names)
    report["modern_character_missing_components"] = sorted(required - component_names)
    if report["modern_character_missing_components"]:
        raise RuntimeError("Modern player is missing RPG/GAS components: " +
                           ", ".join(report["modern_character_missing_components"]))

    active_mode = next(row for row in report["assets"]
                       if row["asset"].endswith("BP_RPGGameModeBase"))
    expected_framework = {
        "default_pawn_class": "/Game/Blueprints/Characters/BP_PlayerCharacter.BP_PlayerCharacter_C",
        "player_controller_class": "/Game/Blueprints/Framework/BP_PlayerController.BP_PlayerController_C",
        "player_state_class": "/Game/Blueprints/Framework/BP_PlayerState.BP_PlayerState_C",
    }
    report["active_game_mode_expected_framework"] = expected_framework
    report["active_game_mode_matches"] = all(
        active_mode.get(name) == value for name, value in expected_framework.items())
    if not report["active_game_mode_matches"]:
        raise RuntimeError("Tutorial RPG game mode is not routed through the unified DT framework")
    report["passed"] = True
except Exception:
    report["error"] = traceback.format_exc()
finally:
    output = Path(unreal.Paths.project_saved_dir()) / "Automation" / "FrameworkUnification.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("FRAMEWORK_UNIFICATION " + json.dumps(report))
    unreal.SystemLibrary.quit_editor()
