"""Export through the existing backed-up retargeter after verifying mesh skeleton bindings."""
import hashlib
import json
from pathlib import Path
import traceback
import unreal

report = {"passed": False, "assigned_to_live_character": False, "visual_pose_validated": False, "outputs": []}
try:
    source = unreal.load_asset("/Game/Characters/Mannequin_UE4/Meshes/SK_Mannequin")
    target = unreal.load_asset("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin")
    expected_source = unreal.load_asset("/Game/Characters/Mannequin_UE4/Meshes/SK_Mannequin_Skeleton")
    if not source or source.get_editor_property("skeleton") != expected_source or not target.get_editor_property("skeleton"):
        raise RuntimeError("Source or target mesh skeleton binding is invalid")
    retargeter = unreal.load_asset("/Game/DTIntegration/Animation/RTG_DT_LegacyUE4_UEFN")
    if not retargeter:
        raise RuntimeError("The staged retargeter is missing")
    report["retargeter"] = retargeter.get_path_name()
    controller = unreal.IKRetargeterController.get_controller(retargeter)
    report["retarget_ops"] = controller.get_num_retarget_ops()
    if report["retarget_ops"] < 1:
        raise RuntimeError("The retargeter has no operations")
    output_folder = Path(unreal.Paths.project_content_dir()) / "DTIntegration/Animation/Combat"
    if output_folder.exists() and any(output_folder.glob("*.uasset")):
        raise RuntimeError("Candidate output folder already contains saved animation; refusing another export")
    original = "/Game/Art/3D/Player/Animations/Anims/Retargeted/Sword_And_Shield_Slash_mixamo_com_Montage"
    original_file = Path(unreal.Paths.project_content_dir()) / (original[len("/Game/"):] + ".uasset")
    before = hashlib.sha256(original_file.read_bytes()).hexdigest()
    inputs = unreal.IKRetargetBatchOperationInputs()
    for prop, value in dict(assets_to_retarget=[unreal.EditorAssetLibrary.find_asset_data(original)], source_mesh=source,
                            target_mesh=target, ik_retarget_asset=retargeter, prefix="DT_", suffix="_UEFN",
                            target_path="/Game/DTIntegration/Animation/Combat", include_referenced_assets=True,
                            overwrite_existing_files=False).items():
        inputs.set_editor_property(prop, value)
    outputs = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    if not outputs:
        raise RuntimeError("No retargeted output")
    for data in outputs:
        asset = data.get_asset()
        matches = asset.get_editor_property("skeleton") == target.get_editor_property("skeleton")
        if not matches or not unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
            raise RuntimeError("Output verification/save failed: " + asset.get_path_name())
        report["outputs"].append({"asset": asset.get_path_name(), "target_skeleton_matches": matches})
    report["source_montage_unchanged"] = hashlib.sha256(original_file.read_bytes()).hexdigest() == before
    report["passed"] = report["source_montage_unchanged"]
except Exception:
    report["exception"] = traceback.format_exc()
    raise
finally:
    output = Path(unreal.Paths.project_saved_dir()) / "Automation/DTCombatRetarget.json"
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("DT_COMBAT_RETARGET " + json.dumps(report))
    unreal.SystemLibrary.quit_editor()
