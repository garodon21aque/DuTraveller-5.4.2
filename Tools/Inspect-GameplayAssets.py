"""Read-only asset types and actual model/skeleton bindings; never saves assets."""
from collections import Counter
import json
from pathlib import Path
import traceback
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
rows = []
models = []
try:
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous(["/Game"], True)
    for data in registry.get_assets_by_path("/Game", recursive=True):
        package = str(data.package_name)
        asset_type = str(data.asset_class_path.asset_name)
        row = dict(asset=package, asset_type=asset_type, tags={})
        for tag in ("ParentClass", "NativeParentClass", "GeneratedClass", "Skeleton", "TargetSkeleton", "RowStructure"):
            try:
                value = data.get_tag_value(tag)
                if value:
                    row["tags"][tag] = str(value)
            except Exception:
                pass
        rows.append(row)
        if asset_type == "SkeletalMesh":
            model = dict(asset=package)
            try:
                mesh = unreal.load_asset(package)
                skeleton = mesh.get_editor_property("skeleton")
                model["skeleton"] = skeleton.get_path_name() if skeleton else None
                model["materials"] = [str(slot.material_interface) for slot in mesh.get_editor_property("materials")]
            except Exception:
                model["load_error"] = traceback.format_exc()
            models.append(model)
    result = dict(asset_counts=dict(Counter(r["asset_type"] for r in rows)), models=models, assets=rows)
    (root / "Docs/GameplayAssetInventory.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    unreal.log("GAMEPLAY_INVENTORY " + json.dumps(dict(asset_counts=result["asset_counts"], models=models)))
    unreal.log("GAMEPLAY_INVENTORY_COMPLETE")
except Exception:
    unreal.log_error(traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
