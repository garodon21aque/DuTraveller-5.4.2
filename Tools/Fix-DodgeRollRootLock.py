"""Make the retargeted dodge roll play truly in-place: the collision capsule owns the
traversal (ConstantForce root motion), so the mesh root must not drift away mid-roll.
Tries the authored ForceRootLock flag first; falls back to flattening the root track
through the animation data model."""
import json,shutil,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir()); report={"passed":False,"steps":[],"errors":[],"method":None,"poses_before":[],"poses_after":[]}
ASSET="/Game/DTIntegration/Animation/Avoidance/DT_Stand_To_Roll_inplace_mixamo_com_UEFN"
def step(name,ok): report["steps"].append({"name":name,"passed":bool(ok)})
def root_xy(seq,fraction):
    pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(seq,seq.get_play_length()*fraction,unreal.AnimPoseEvaluationOptions())
    tr=unreal.AnimPoseExtensions.get_bone_pose(pose,"root",unreal.AnimPoseSpaces.WORLD).translation
    return (round(tr.x,2),round(tr.y,2),round(tr.z,2))
def drift(poses):
    return max(abs(a-b) for p in poses for a,b in zip(p,poses[0]))
def flatten_root(seq):
    # Flatten only translation: the somersault lives partly in root rotation, and the
    # ConstantForce capsule supplies traversal. The body must pivot around a fixed root.
    controller=getattr(seq,"controller",None)
    model=getattr(seq,"data_model",None) or (controller.get_model_interface() if controller else None)
    if not controller or not model: raise RuntimeError("Animation data model/controller unavailable")
    frames=model.get_number_of_frames()
    if frames<2: raise RuntimeError("Sequence has too few frames")
    poses=[unreal.AnimationLibrary.get_bone_pose_for_frame(seq,"root",f,False) for f in range(frames)]
    first=poses[0].translation
    pos=[first]*frames
    rot=[p.rotation for p in poses]
    scale=[p.scale3d for p in poses]
    controller.open_bracket("Lock dodge roll root translation to first frame")
    controller.set_bone_track_keys("root",pos,rot,scale)
    controller.close_bracket()
try:
    source=root/("Content/"+ASSET.removeprefix("/Game/").split(".")[0]+".uasset")
    dest=root/"Saved/RestorationBackups/DodgeRootLock"/source.relative_to(root)
    if source.exists() and not dest.exists(): dest.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(source,dest)
    step("Sequence backup preserved",dest.exists())
    seq=unreal.load_asset(ASSET)
    if not seq: raise RuntimeError("Retargeted roll sequence missing")
    fractions=[0,.25,.5,.75,1]
    report["poses_before"]=[root_xy(seq,f) for f in fractions]
    # Attempt 1: authored lock keeps the root pinned to its first frame during playback.
    seq.set_editor_property("force_root_lock",True)
    seq.set_editor_property("root_motion_root_lock",unreal.RootMotionRootLock.REF_POSE)
    after=[root_xy(seq,f) for f in fractions]
    if drift(after)<1.0:
        report["method"]="force_root_lock"; report["poses_after"]=after
    else:
        # Attempt 2: rewrite the root track so every key matches the first frame.
        flatten_root(seq)
        report["method"]="flatten_root_track"; report["poses_after"]=[root_xy(seq,f) for f in fractions]
    step("Root bone remains at the first frame throughout the roll",drift(report["poses_after"])<1.0)
    # Root motion extraction stays disabled: the ConstantForce path owns the capsule.
    step("Sequence still reports no extractable root motion",seq.get_editor_property("enable_root_motion")==False)
    if not unreal.EditorAssetLibrary.save_loaded_asset(seq,False): raise RuntimeError("Sequence save failed")
    step("Sequence saved",True)
    report["passed"]=all(s["passed"] for s in report["steps"])
except Exception:
    report["errors"].append(traceback.format_exc()); unreal.log_error(report["errors"][-1])
finally:
    (root/"Saved/Automation/DodgeRootLock.json").write_text(json.dumps(report,indent=2,default=str))
    unreal.SystemLibrary.quit_editor()
