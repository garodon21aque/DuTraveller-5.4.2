"""Read-only Blueprint parent, component, framework and provenance inventory."""
import json
from pathlib import Path
import traceback
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
historical = set(json.loads((root / "Tools/HistoricalAssetPaths.json").read_text(encoding="utf-8-sig")))
rows = []

try:
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous(["/Game/Blueprints"], True)
    assets = registry.get_assets_by_path("/Game/Blueprints", recursive=True)
    for data in assets:
        package = str(data.package_name)
        name = str(data.asset_name)
        if not (package.startswith(("/Game/Blueprints/AI/", "/Game/Blueprints/Player/", "/Game/Blueprints/Characters/"))
                or any(word in name.lower() for word in ("character", "controller", "gamemode", "playerstate", "hud"))):
            continue
        row = dict(asset=package, historical_asset=("Content/" + package[len("/Game/"):] + ".uasset") in historical)
        try:
            asset = unreal.load_asset(package)
            if not isinstance(asset, unreal.Blueprint):
                continue
            parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(asset)
            row["direct_parent"] = parent.get_path_name() if parent else None
            cls = asset.generated_class()
            default = unreal.get_default_object(cls) if cls else None
            if isinstance(default, unreal.AIController):
                row["native_role"] = "AI controller"
            elif isinstance(default, unreal.PlayerController):
                row["native_role"] = "player controller"
            elif isinstance(default, unreal.Character):
                row["native_role"] = "character (player/AI role requires possession and reference evidence)"
            row["properties"] = {}
            for prop in ("default_pawn_class", "player_controller_class", "player_state_class", "hud_class",
                         "ai_controller_class", "auto_possess_ai", "auto_possess_player"):
                try:
                    value = default.get_editor_property(prop)
                    row["properties"][prop] = value.get_path_name() if isinstance(value, unreal.Object) else str(value)
                except Exception:
                    pass
            if isinstance(default, unreal.Actor):
                row["native_components"] = [c.get_class().get_path_name() for c in default.get_components_by_class(unreal.ActorComponent)]
            row["blueprint_member_variables"] = [str(v) for v in unreal.BlueprintEditorLibrary.list_member_variable_names(asset)]
        except Exception:
            row["inspection_error"] = traceback.format_exc()
        rows.append(row)
        unreal.log("CHARACTER_HIERARCHY " + json.dumps(row))
    output = root / "Docs/CharacterHierarchy.json"
    output.write_text(json.dumps(rows, indent=2), encoding="utf-8")
    unreal.log("CHARACTER_HIERARCHY_COMPLETE " + str(output))
except Exception:
    unreal.log_error(traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
