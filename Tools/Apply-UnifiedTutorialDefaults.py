"""Route the restored RPG game mode through the modern DT player framework.

The Tutorial map already selects BP_RPGGameModeBase.  Changing that Blueprint's
class defaults preserves the map and RPG game-mode contract while ensuring PIE
possesses the DT Motion Matching/traversal character and its DT controller/state
subclasses.  Those native DT classes now inherit the restored RPG/GAS layer.
"""
import json
import unreal


GAME_MODE = "/Game/Blueprints/Framework/BP_RPGGameModeBase"
CLASS_DEFAULTS = {
    "default_pawn_class": "/Game/Blueprints/Characters/BP_PlayerCharacter.BP_PlayerCharacter_C",
    "player_controller_class": "/Game/Blueprints/Framework/BP_PlayerController.BP_PlayerController_C",
    "player_state_class": "/Game/Blueprints/Framework/BP_PlayerState.BP_PlayerState_C",
}


def class_path(value):
    return value.get_path_name() if value else None


try:
    blueprint = unreal.load_asset(GAME_MODE)
    if not blueprint:
        raise RuntimeError("Unable to load " + GAME_MODE)

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    status = str(blueprint.get_editor_property("status"))
    if "ERROR" in status.upper():
        raise RuntimeError("Refusing to change a failing game-mode Blueprint: " + status)

    defaults = unreal.get_default_object(blueprint.generated_class())
    before = {}
    after = {}
    for property_name, asset_path in CLASS_DEFAULTS.items():
        before[property_name] = class_path(defaults.get_editor_property(property_name))
        target_class = unreal.load_class(None, asset_path)
        if not target_class:
            raise RuntimeError("Unable to load class " + asset_path)
        defaults.set_editor_property(property_name, target_class)
        after[property_name] = class_path(defaults.get_editor_property(property_name))

    if after != CLASS_DEFAULTS:
        raise RuntimeError("Class-default verification failed: " + json.dumps(after))
    if not unreal.DTStructRepairLibrary.save_asset_package(blueprint.get_path_name()):
        raise RuntimeError("Game-mode package was not saved")

    unreal.log("UNIFIED_TUTORIAL_DEFAULTS " + json.dumps({
        "asset": GAME_MODE,
        "before": before,
        "after": after,
        "status": status,
    }))
finally:
    unreal.SystemLibrary.quit_editor()
