"""Move retained money item Blueprints onto the production currency parent."""

import json
from pathlib import Path
import unreal


ASSETS = (
    "/Game/Blueprints/InventorySystem/Items/MoneyItem/BP_BronzeItem",
    "/Game/Blueprints/InventorySystem/Items/MoneyItem/BP_GoldItem",
)

results = []
parent = unreal.Currency_ItemObject

for asset_path in ASSETS:
    blueprint = unreal.load_asset(asset_path)
    if not blueprint:
        results.append({"asset": asset_path, "status": "missing", "saved": False})
        continue
    unreal.BlueprintEditorLibrary.reparent_blueprint(blueprint, parent)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    status = str(blueprint.get_editor_property("status"))
    saved = "BS_UP_TO_DATE" in status and unreal.EditorAssetLibrary.save_loaded_asset(
        blueprint, only_if_is_dirty=False)
    results.append({"asset": asset_path, "parent": str(parent),
                    "status": status, "saved": bool(saved)})

report = Path(unreal.Paths.project_saved_dir()) / "Automation" / "CurrencyItemHierarchyRepair.json"
report.parent.mkdir(parents=True, exist_ok=True)
report.write_text(json.dumps(results, indent=2), encoding="utf-8")
unreal.log("CURRENCY_ITEM_HIERARCHY_REPAIR " + json.dumps(results))
unreal.SystemLibrary.quit_editor()
