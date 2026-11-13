"""Gate WBP_LevelUpNotification creation on the delegate's bLevelUp flag and audit the dodge-roll asset chain."""
import json,shutil,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir()); report={"passed":False,"steps":[],"errors":[],"dodge":{}}
helper=unreal.DTStructRepairLibrary
def step(name,ok): report["steps"].append({"name":name,"passed":bool(ok)})
try:
    hud="Content/UI/HUD/WBP_HUD.uasset"
    source=root/hud; dest=root/"Saved/RestorationBackups/HUDLevelUpGate"/hud
    if source.exists() and not dest.exists(): dest.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(source,dest)
    step("HUD backup preserved",dest.exists())
    step("Level-up gate branch inserted",helper.repair_hud_level_up("/Game/UI/HUD/WBP_HUD"))
    step("HUD compiles and saves",helper.force_compile_and_save("/Game/UI/HUD/WBP_HUD"))
    helper.dump_blueprint_graph("/Game/UI/HUD/WBP_HUD.WBP_HUD")
    # Dodge-roll chain: montage -> retargeted sequence -> root motion -> player CDO -> AnimBP mode.
    montage=unreal.load_asset("/Game/DTIntegration/Animation/Avoidance/AM_DT_DodgeRoll")
    if montage:
        tracks=[{"slot":str(t.slot_name),"segments":len(t.anim_track.anim_segments)} for t in montage.slot_anim_tracks]
        seq=None
        if tracks and tracks[0]["segments"]:
            segment=montage.slot_anim_tracks[0].anim_track.anim_segments[0]
            seq=getattr(segment,"anim_reference",None)
            if seq is None:
                try: seq=segment.get_editor_property("anim_reference")
                except Exception: seq=None
        try: root_motion=montage.get_editor_property("bEnableRootMotion")
        except Exception: root_motion=None
        report["dodge"]["montage"]={"has_root_motion":root_motion,"length":montage.get_play_length(),
            "auto_blend_out":montage.get_editor_property("enable_auto_blend_out"),"slot_tracks":tracks,
            "sequence":seq.get_path_name() if seq else None}
        if seq:
            report["dodge"]["sequence"]={"enable_root_motion":seq.get_editor_property("enable_root_motion"),
                "force_root_lock":seq.get_editor_property("force_root_lock"),
                "root_motion_root_lock":str(seq.get_editor_property("root_motion_root_lock")),
                "length":seq.get_play_length()}
    bp=unreal.load_asset("/Game/Blueprints/Characters/BP_PlayerCharacter")
    cdo=unreal.get_default_object(bp.generated_class())
    dodge=cdo.get_editor_property("dodge_animation")
    report["dodge"]["player"]={"montage":str(dodge.get_editor_property("montage")),
        "play_rate":dodge.get_editor_property("play_rate"),"dodge_distance":cdo.get_editor_property("dodge_distance"),
        "dodge_duration":cdo.get_editor_property("dodge_duration"),"enable_dodge":cdo.get_editor_property("enable_dodge")}
    abp=unreal.load_asset("/Game/Blueprints/Animations/ABP_SandboxCharacter")
    report["dodge"]["anim_bp"]={"root_motion_mode":str(unreal.get_default_object(abp.generated_class()).get_editor_property("root_motion_mode"))}
    report["passed"]=all(s["passed"] for s in report["steps"])
except Exception:
    report["errors"].append(traceback.format_exc()); unreal.log_error(report["errors"][-1])
finally:
    (root/"Saved/Automation/HUDLevelUpGate.json").write_text(json.dumps(report,indent=2,default=str))
    unreal.SystemLibrary.quit_editor()
