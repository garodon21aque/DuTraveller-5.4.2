"""Complete hard/soft-reference remapping in the backed-up recovered assets."""
import json
from pathlib import Path
import unreal

path = Path(unreal.Paths.project_saved_dir()) / "Automation/LegacyMannequinRecovery.json"
report = json.loads(path.read_text(encoding="utf-8"))
report["references_persisted"] = False
try:
    if not unreal.DTAnimationMigrationLibrary.finalize_legacy_mannequin(report["package_map"]):
        raise RuntimeError("Reference remapping failed")
    for target in report["package_map"].values():
        asset = unreal.load_asset(target)
        if not asset or not unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
            raise RuntimeError("Failed to save " + target)
    skeleton = unreal.load_asset(report["restored_skeleton"])
    mesh = unreal.load_asset(report["restored_mesh"])
    report["mesh_skeleton_matches"] = mesh.get_editor_property("skeleton") == skeleton
    if not report["mesh_skeleton_matches"]:
        raise RuntimeError("Preview mesh still has an invalid skeleton")
    report["references_persisted"] = True
finally:
    path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.SystemLibrary.quit_editor()
