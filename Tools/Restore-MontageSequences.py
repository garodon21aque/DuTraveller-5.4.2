"""Restore sequences omitted by the earlier montage-only retarget export."""
import json
import shutil
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
report = {"passed":False,"repairs":[],"errors":[]}
previous=root/"Saved/Automation/MontageSequenceRepair.json"
if previous.exists(): report["repairs"]=json.loads(previous.read_text(encoding="utf-8")).get("repairs",[])
originals = [
    ("/Game/Art/3D/Player/Animations/Anims/Retargeted/Standing_1H_Cast_Spell_01_mixamo_com_Montage", "DT_Standing_1H_Cast_Spell_01_UEFN"),
    ("/Game/Art/3D/Player/Animations/Anims/Retargeted/Standing_Melee_Attack_Horizontal_mixamo_com_Montage", "DT_Standing_Melee_Attack_Horizontal_UEFN"),
    ("/Game/Art/3D/Player/Animations/Anims/Retargeted/Standing_Melee_Attack_360_High_mixamo_com_Montage", "DT_Standing_Melee_Attack_360_High_UEFN"),
    ("/Game/Art/3D/Ai/Enemy/Anims/AMAI_ReactHitFont", "DT_AMAI_ReactHitFont_UEFN"),
]
probe = unreal.DTStructRepairLibrary
try:
    source_mesh = unreal.load_asset("/Game/Characters/Mannequin_UE4/Meshes/SK_Mannequin")
    target_mesh = unreal.load_asset("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin")
    retargeter = unreal.load_asset("/Game/DTIntegration/Animation/RTG_DT_LegacyUE4_UEFN")
    for original, name in originals:
        target = "/Game/DTIntegration/Animation/Combat/" + name
        before = list(probe.repair_duplicate_montage_slots(target,False))
        if not any("sequence=None" in line for line in before): continue
        refs = [line.split("sequence=",1)[1].split(" start=",1)[0] for line in probe.repair_duplicate_montage_slots(original,False) if "sequence=" in line and "sequence=None" not in line]
        if len(set(refs)) != 1: raise RuntimeError("Expected one original sequence for " + original + ": " + str(refs))
        sequence_path = "/Game/DTIntegration/Animation/Combat/DT_" + refs[0].rsplit(".",1)[-1] + "_UEFN"
        if not unreal.EditorAssetLibrary.does_asset_exist(sequence_path):
            inputs = unreal.IKRetargetBatchOperationInputs()
            for prop,value in dict(assets_to_retarget=[unreal.EditorAssetLibrary.find_asset_data(refs[0])],source_mesh=source_mesh,target_mesh=target_mesh,ik_retarget_asset=retargeter,prefix="DT_",suffix="_UEFN",target_path="/Game/DTIntegration/Animation/Combat",include_referenced_assets=False,overwrite_existing_files=False).items():
                inputs.set_editor_property(prop,value)
            outputs = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
            if not outputs: raise RuntimeError("Retarget failed " + refs[0])
        sequence = unreal.load_asset(sequence_path)
        if not isinstance(sequence,unreal.AnimSequence): raise RuntimeError("Missing retarget output " + sequence_path)
        if not unreal.EditorAssetLibrary.save_loaded_asset(sequence,False): raise RuntimeError("Sequence save failed")
        assetfile=root/("Content/"+target[len("/Game/"):]+".uasset")
        backup=root/"Saved/Backups/MontageMissingSequences"/assetfile.relative_to(root)
        backup.parent.mkdir(parents=True,exist_ok=True)
        if not backup.exists(): shutil.copy2(assetfile,backup)
        if not probe.restore_missing_montage_sequence(target,sequence_path): raise RuntimeError("Relink failed " + target)
        after=list(probe.repair_duplicate_montage_slots(target,False))
        if any("sequence=None" in line for line in after): raise RuntimeError("Missing sequence remains " + target)
        report["repairs"].append(dict(montage=target,source=refs[0],sequence=sequence_path,before=before,after=after))
    # The authored hold-to-sprint action sets its flag on Triggered but did not
    # clear it on release/cancel. Reuse that setter with the released false value.
    bp="/Game/Blueprints/Characters/BP_PlayerCharacter"
    assetfile=root/"Content/Blueprints/Characters/BP_PlayerCharacter.uasset"
    backup=root/"Saved/Backups/SprintRelease"/assetfile.relative_to(root)
    backup.parent.mkdir(parents=True,exist_ok=True)
    if not backup.exists(): shutil.copy2(assetfile,backup)
    for pin in ("Completed","Canceled"):
        if not probe.connect_blueprint_pins(bp,"K2Node_EnhancedInputAction_5",pin,"K2Node_VariableSet_4","execute"):
            raise RuntimeError("Sprint release connection failed")
    if not probe.force_compile_and_save(bp): raise RuntimeError("Player Blueprint compile failed")
    report["sprint_release_repaired"]=True
    for bp in ("/Game/UI/HUD/SpellGlobes/AttributeGlobes/WBP_ManaGlobe", "/Game/UI/HUD/SpellGlobes/AttributeGlobes/WBP_ProgressGlobe"):
        probe.dump_blueprint_graph(bp)
    report["passed"]=True
except Exception:
    import traceback
    report["errors"].append(traceback.format_exc())
    unreal.log_error(report["errors"][-1])
finally:
    (root/"Saved/Automation/MontageSequenceRepair.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
    unreal.log("MONTAGE_SEQUENCE_REPAIR " + json.dumps(report))
