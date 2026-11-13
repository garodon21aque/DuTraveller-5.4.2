"""Read existing roll content and reproduce move/cast/stop transitions without saving assets."""
import json,time,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir()); out=root/"Saved/Automation/AvoidanceLocomotionAudit.json"
report={"rolls":[],"samples":[],"errors":[],"input":{}}
probe=unreal.DTStructRepairLibrary
for path in unreal.EditorAssetLibrary.list_assets("/Game/Art/3D/Player/Animations",True,False):
    if "roll" not in path.lower(): continue
    asset=unreal.load_asset(path)
    if not isinstance(asset,unreal.AnimSequenceBase): continue
    row={"path":asset.get_path_name(),"skeleton":str(asset.get_editor_property("skeleton")),"length":asset.get_play_length()}
    if isinstance(asset,unreal.AnimSequence):
        row["root_motion"]=asset.get_editor_property("enable_root_motion")
        try:
            pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(asset,asset.get_play_length()*.4,unreal.AnimPoseEvaluationOptions())
            row["head"]=str(unreal.AnimPoseExtensions.get_bone_pose(pose,"head",unreal.AnimPoseSpaces.WORLD))
        except Exception as exc: row["pose_error"]=str(exc)
    report["rolls"].append(row)
for path in ("/Game/Blueprints/Characters/BP_PlayerCharacter","/Game/Blueprints/Animations/ABP_SandboxCharacter"):
    probe.dump_blueprint_graph(path)
mapping=unreal.load_asset("/Game/Input/IMC_GDCMotionMatching")
report["input"]["mappings"]=[str(m) for m in mapping.get_editor_property("mappings")]
unreal.EditorLoadingAndSavingUtils.load_map("/Game/Levels/Tutorial")
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
worlds=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem); editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
started=time.monotonic(); flags=set(); callback=None; last=-1
def tick(delta):
    global last
    try:
        if time.monotonic()-started>180: raise RuntimeError("Audit timeout")
        world=worlds.get_game_world()
        if not world: return
        pc=unreal.GameplayStatics.get_player_controller(world,0); pawn=pc.get_controlled_pawn() if pc else None
        if not pawn: return
        t=unreal.GameplayStatics.get_time_seconds(world)
        if "init" not in flags:
            for enemy in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RPGAICharacter):
                if enemy.get_controller(): enemy.get_controller().set_actor_tick_enabled(False)
            flags.add("init")
        if 1<t<4 or 7<t<9 or 12<t<14: probe.inject_runtime_input(pc,"IA_Move",unreal.Vector(1,0,0))
        if 2<t<4: probe.inject_runtime_input(pc,"IA_Sprint",unreal.Vector(1,0,0))
        if t>3 and "cast" not in flags:
            probe.inject_runtime_input(pc,"One",unreal.Vector(1,0,0)); flags.add("cast")
        if t>7.5 and "melee" not in flags:
            probe.inject_runtime_input(pc,"LeftMouseButton",unreal.Vector(1,0,0)); flags.add("melee")
        if t>12.5 and "ui" not in flags: pc.open_ui_panel("Attributes"); flags.add("ui")
        if t>14.5 and "close" not in flags: pc.close_ui_panels(); flags.add("close")
        if t-last>.2:
            report["samples"].append({"time":t,**dict(probe.inspect_runtime_player(pc))}); last=t
            out.write_text(json.dumps(report,indent=2))
        if t>18:
            unreal.unregister_slate_post_tick_callback(callback); out.write_text(json.dumps(report,indent=2))
            unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    except Exception:
        report["errors"].append(traceback.format_exc()); out.write_text(json.dumps(report,indent=2))
        unreal.unregister_slate_post_tick_callback(callback); unreal.EditorPythonScripting.set_keep_python_script_alive(False)
callback=unreal.register_slate_post_tick_callback(tick); editor.editor_request_begin_play()
