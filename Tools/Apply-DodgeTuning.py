"""Apply the production dodge timing, distance, and full-animation immunity defaults."""
import json
import shutil
import traceback
from pathlib import Path

import unreal

root = Path(unreal.Paths.project_dir())
asset_path = "/Game/Blueprints/Characters/BP_PlayerCharacter"
report_path = root / "Saved/Automation/DodgeTuning.json"
report = {"passed": False, "asset": asset_path, "errors": []}

try:
    source = root / "Content/Blueprints/Characters/BP_PlayerCharacter.uasset"
    backup = root / "Saved/RestorationBackups/DodgeTuning/BP_PlayerCharacter.uasset"
    if source.exists() and not backup.exists():
        backup.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, backup)

    blueprint = unreal.load_asset(asset_path)
    if not blueprint:
        raise RuntimeError("BP_PlayerCharacter could not be loaded")
    defaults = unreal.get_default_object(blueprint.generated_class())
    defaults.set_editor_property("dodge_duration", 1.0)
    defaults.set_editor_property("dodge_distance", 500.0)
    defaults.set_editor_property("dodge_invulnerability", True)
    if not unreal.DTStructRepairLibrary.force_compile_and_save(asset_path):
        raise RuntimeError("BP_PlayerCharacter compile/save failed")

    report.update({
        "passed": True,
        "dodge_duration": defaults.get_editor_property("dodge_duration"),
        "dodge_distance": defaults.get_editor_property("dodge_distance"),
        "dodge_invulnerability": defaults.get_editor_property("dodge_invulnerability"),
    })
except Exception:
    report["errors"].append(traceback.format_exc())
    unreal.log_error(report["errors"][-1])
finally:
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2))
