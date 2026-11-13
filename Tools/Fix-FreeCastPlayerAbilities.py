"""Repair player attacks that incorrectly require a locked target; verify real assets."""
import json
import shutil
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
report = {"passed": False, "abilities": [], "errors": []}
paths = ["/Game/Data/Abilities/Player/DA_Player_" + name
         for name in ("FireBolt", "IceShards", "BasicSlash", "FullSwing")]
try:
    for path in paths:
        ability = unreal.load_asset(path)
        if not ability:
            raise RuntimeError("Missing player ability: " + path)
        before = bool(ability.get_editor_property("requires_target"))
        file = root / ("Content/" + path[len("/Game/"):] + ".uasset")
        backup = root / "Saved/Backups/FreeCastPlayerAbilities" / file.relative_to(root)
        if before:
            backup.parent.mkdir(parents=True, exist_ok=True)
            if not backup.exists():
                shutil.copy2(file, backup)
            ability.set_editor_property("requires_target", False)
            if not unreal.EditorAssetLibrary.save_loaded_asset(ability, False):
                raise RuntimeError("Unable to save " + path)
        report["abilities"].append({"asset": path, "required_target_before": before,
                                    "requires_target_after": bool(ability.get_editor_property("requires_target")),
                                    "backup": str(backup) if backup.exists() else None})
    report["passed"] = all(not row["requires_target_after"] for row in report["abilities"])
except Exception as exc:
    report["errors"].append(str(exc))
finally:
    output = root / "Saved/Automation/FreeCastPlayerAbilities.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("FREE_CAST_RESULT " + json.dumps(report))
    if not report["passed"]:
        unreal.log_error("Free cast repair failed")
