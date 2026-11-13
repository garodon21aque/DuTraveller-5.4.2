"""Reuse the recovered roll, preserve DT locomotion, and author editable avoidance defaults."""
import json,shutil,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir()); report={"passed":False,"assets":[],"errors":[],"combat_montages":[]}
tools=unreal.AssetToolsHelpers.get_asset_tools(); helper=unreal.DTStructRepairLibrary
def backup(path):
    source=root/("Content/"+path.removeprefix("/Game/").split(".")[0]+".uasset")
    dest=root/"Saved/RestorationBackups/DodgeFocus"/source.relative_to(root)
    if source.exists() and not dest.exists(): dest.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(source,dest)
def save(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset,False): raise RuntimeError("Save failed "+asset.get_path_name())
    report["assets"].append(asset.get_path_name())
try:
    source=unreal.load_asset("/Game/Characters/Mannequin_UE4/Meshes/SK_Mannequin")
    target=unreal.load_asset("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin")
    folder="/Game/DTIntegration/Animation/Avoidance"
    sequence_path=folder+"/DT_Stand_To_Roll_UEFN"
    sequence=unreal.load_asset(sequence_path)
    if not sequence:
        inputs=unreal.IKRetargetBatchOperationInputs()
        values=dict(assets_to_retarget=[unreal.EditorAssetLibrary.find_asset_data("/Game/Art/3D/Player/Animations/Anims/Retargeted/RootMixamoUE4/Stand_To_Roll")],source_mesh=source,target_mesh=target,ik_retarget_asset=unreal.load_asset("/Game/DTIntegration/Animation/RTG_DT_LegacyUE4_UEFN"),prefix="DT_",suffix="_UEFN",target_path=folder,include_referenced_assets=False,overwrite_existing_files=False)
        for name,value in values.items(): inputs.set_editor_property(name,value)
        outputs=unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
        report["retarget_outputs"]=[str(item.object_path) if hasattr(item,"object_path") else str(item) for item in outputs]
        sequence=unreal.load_asset(sequence_path)
    if not sequence or sequence.get_editor_property("skeleton")!=target.get_editor_property("skeleton"): raise RuntimeError("Roll retarget missing or incompatible")
    sequence.set_editor_property("enable_root_motion",True)
    sequence.set_editor_property("force_root_lock",True)
    sequence.set_editor_property("root_motion_root_lock",unreal.RootMotionRootLock.ANIM_FIRST_FRAME)
    save(sequence)
    path=folder+"/AM_DT_DodgeRoll_RootMotion"
    montage=unreal.load_asset(path)
    if not montage:
        factory=unreal.AnimMontageFactory(); factory.set_editor_property("target_skeleton",target.get_editor_property("skeleton")); factory.set_editor_property("source_animation",sequence)
        montage=tools.create_asset("AM_DT_DodgeRoll_RootMotion",folder,unreal.AnimMontage,factory)
    montage.set_editor_property("enable_auto_blend_out",True)
    for prop,duration in (("blend_in",.08),("blend_out",.1)):
        blend=montage.get_editor_property(prop); blend.set_editor_property("blend_time",duration); montage.set_editor_property(prop,blend)
    save(montage)
    if not helper.set_montage_slot_name(path,"DefaultSlot"): raise RuntimeError("Could not configure full-body roll slot")
    for bp_path in ("/Game/Blueprints/Characters/BP_PlayerCharacter","/Game/Blueprints/Animations/ABP_SandboxCharacter"):
        backup(bp_path)
    abp="/Game/Blueprints/Animations/ABP_SandboxCharacter"
    if not helper.repair_combat_upper_body_blend(abp) or not helper.force_compile_and_save(abp): raise RuntimeError("Continuous locomotion slot repair failed")
    anim_bp=unreal.load_asset(abp)
    unreal.get_default_object(anim_bp.generated_class()).set_editor_property("root_motion_mode",unreal.RootMotionMode.ROOT_MOTION_FROM_MONTAGES_ONLY)
    if not helper.force_compile_and_save(abp): raise RuntimeError("Root-motion AnimBP compile failed")
    report["assets"].append(abp)
    bp=unreal.load_asset("/Game/Blueprints/Characters/BP_PlayerCharacter"); cdo=unreal.get_default_object(bp.generated_class())
    entry=unreal.DTCombatAnimation(); entry.set_editor_property("montage",montage); entry.set_editor_property("play_rate",1.0)
    cdo.set_editor_property("dodge_animation",entry)
    cdo.set_editor_property("dodge_duration",1.0)
    cdo.set_editor_property("dodge_distance",500.0)
    cdo.set_editor_property("dodge_invulnerability",True)
    mesh=cdo.get_editor_property("mesh")
    mesh.set_editor_property("visibility_based_anim_tick_option",unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
    mesh.set_editor_property("enable_update_rate_optimizations",False)
    for entry in cdo.get_editor_property("animation_router").get_editor_property("combat_animations").values():
        clip=entry.get_editor_property("montage")
        if clip: report["combat_montages"].append({"path":clip.get_path_name(),"auto_blend_out":clip.get_editor_property("enable_auto_blend_out")})
    if not helper.force_compile_and_save("/Game/Blueprints/Characters/BP_PlayerCharacter"): raise RuntimeError("Player Blueprint compile failed")
    report["assets"].append(bp.get_path_name()); report["passed"]=True
except Exception:
    report["errors"].append(traceback.format_exc()); unreal.log_error(report["errors"][-1])
finally:
    (root/"Saved/Automation/DodgeFocusPreparation.json").write_text(json.dumps(report,indent=2))



