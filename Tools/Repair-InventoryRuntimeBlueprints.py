"""Refresh the retained inventory drag/drop graph after parent contract restoration."""

import json
from pathlib import Path
import unreal


assets = [
    "/Game/UI/InventorySystem/WBP_DropWidget",
    "/Game/UI/InventorySystem/WBP_PlayerInventory",
]
results = []

for path in assets:
    name = path.rsplit("/", 1)[-1]
    object_path = f"{path}.{name}"
    refreshed = unreal.DTStructRepairLibrary.refresh_blueprint_nodes(object_path)
    blueprint = unreal.load_asset(path)
    if blueprint:
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        status = str(blueprint.get_editor_property("status"))
        saved = "BS_UP_TO_DATE" in status and unreal.EditorAssetLibrary.save_loaded_asset(
            blueprint, only_if_is_dirty=False)
    else:
        status = "missing"
        saved = False
    results.append({"asset": path, "refreshed": bool(refreshed),
                    "status": status, "saved": bool(saved)})

report = Path(unreal.Paths.project_saved_dir()) / "Automation" / "InventoryRuntimeBlueprintRepair.json"
report.parent.mkdir(parents=True, exist_ok=True)
report.write_text(json.dumps(results, indent=2), encoding="utf-8")
unreal.log("INVENTORY_RUNTIME_BLUEPRINT_REPAIR " + json.dumps(results))
unreal.SystemLibrary.quit_editor()
