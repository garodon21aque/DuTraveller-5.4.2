"""Create new UE4-to-UEFN retarget assets and a slash candidate; never edit source animation or live player defaults."""
import hashlib
import json
from pathlib import Path
import traceback
import unreal

report = {"passed": False, "assigned_to_live_character": False, "visual_pose_validated": False, "created": []}
folder = "/Game/DTIntegration/Animation"
tools = unreal.AssetToolsHelpers.get_asset_tools()

def create(name, cls, factory):
    path = folder + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        raise RuntimeError("Refusing to overwrite existing retarget asset: " + path)
    asset = tools.create_asset(name, folder, cls, factory)
    if not asset:
        raise RuntimeError("Failed to create " + path)
    report["created"].append(asset.get_path_name())
    return asset

def save(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
        raise RuntimeError("Failed to save " + asset.get_path_name())

try:
    source = unreal.load_asset("/Game/Characters/Mannequin_UE4/Meshes/SK_Mannequin")
    target = unreal.load_asset("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin")
    if not source or not target:
        raise RuntimeError("Source or target preview mesh is missing")
    if not source.get_editor_property("skeleton") or not target.get_editor_property("skeleton"):
        raise RuntimeError("Source or target mesh is missing its skeleton; refusing batch export")
    rigs = []
    report["rigs"] = []
    for name, mesh in (("IK_DT_LegacyUE4", source), ("IK_DT_UEFN", target)):
        rig = create(name, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
        controller = unreal.IKRigController.get_controller(rig)
        if not controller.set_skeletal_mesh(mesh) or not controller.apply_auto_generated_retarget_definition():
            raise RuntimeError("Automatic retarget definition failed: " + name)
        fbik = controller.apply_auto_fbik()
        chains = controller.get_retarget_chains()
        if len(chains) < 10:
            raise RuntimeError("Insufficient retarget chains: " + name)
        report["rigs"].append({"asset": rig.get_path_name(), "chains": len(chains), "fbik_generated": fbik})
        save(rig)
        rigs.append(rig)
    retargeter = create("RTG_DT_LegacyUE4_UEFN", unreal.IKRetargeter, unreal.IKRetargetFactory())
    controller = unreal.IKRetargeterController.get_controller(retargeter)
    controller.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, rigs[0])
    controller.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, rigs[1])
    controller.set_preview_mesh(unreal.RetargetSourceOrTarget.SOURCE, source)
    controller.set_preview_mesh(unreal.RetargetSourceOrTarget.TARGET, target)
    controller.add_default_ops()
    controller.auto_map_chains(unreal.AutoMapChainType.EXACT, True)
    controller.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
    save(retargeter)

    original = "/Game/Art/3D/Player/Animations/Anims/Retargeted/Sword_And_Shield_Slash_mixamo_com_Montage"
    original_file = Path(unreal.Paths.project_content_dir()) / (original[len("/Game/"):] + ".uasset")
    original_hash = hashlib.sha256(original_file.read_bytes()).hexdigest()
    inputs = unreal.IKRetargetBatchOperationInputs()
    for prop, value in dict(assets_to_retarget=[unreal.EditorAssetLibrary.find_asset_data(original)],
                            source_mesh=source, target_mesh=target, ik_retarget_asset=retargeter,
                            prefix="DT_", suffix="_UEFN", target_path=folder + "/Combat",
                            include_referenced_assets=True, overwrite_existing_files=False).items():
        inputs.set_editor_property(prop, value)
    outputs = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    if not outputs:
        raise RuntimeError("Batch retarget produced no assets")
    report["outputs"] = []
    target_skeleton = target.get_editor_property("skeleton")
    for data in outputs:
        asset = data.get_asset()
        skeleton = asset.get_editor_property("skeleton")
        matches = skeleton == target_skeleton
        report["outputs"].append({"asset": asset.get_path_name(), "target_skeleton_matches": matches})
        if not matches:
            raise RuntimeError("Retarget output has the wrong skeleton")
        save(asset)
    report["source_montage_unchanged"] = hashlib.sha256(original_file.read_bytes()).hexdigest() == original_hash
    report["passed"] = report["source_montage_unchanged"]
except Exception:
    report["exception"] = traceback.format_exc()
    raise
finally:
    output = Path(unreal.Paths.project_saved_dir()) / "Automation/DTCombatRetarget.json"
    output.write_text(json.dumps(report, indent=2, default=str), encoding="utf-8")
    unreal.log("DT_COMBAT_RETARGET " + json.dumps(report, default=str))
    unreal.SystemLibrary.quit_editor()
