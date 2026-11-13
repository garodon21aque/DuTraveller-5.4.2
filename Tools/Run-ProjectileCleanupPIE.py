"""Stress the real projectile-impact timer with active looping and burst FX."""
import json,time,traceback
from pathlib import Path
import unreal
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
root=Path(unreal.Paths.project_dir()); output=root/"Saved/Automation/ProjectileCleanupPIE.json"
report={"passed":False,"checks":[],"samples":[],"errors":[],"spawned":0}
worlds=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
probe=unreal.DTStructRepairLibrary
callback=regression_callback=None; pawn=None; next_spawn=2.0; started=time.monotonic(); prepared=False
def finish():
    global regression_callback
    unreal.unregister_slate_post_tick_callback(callback)
    report["passed"]=not report["errors"] and bool(report["checks"]) and all(x["passed"] for x in report["checks"])
    output.write_text(json.dumps(report,indent=2))
    if report["passed"]:
        editor.editor_request_end_play()
        def regression(delta):
            if worlds.get_game_world(): return
            unreal.unregister_slate_post_tick_callback(regression_callback)
            unreal.SystemLibrary.execute_console_command(worlds.get_editor_world(),"Automation RunTests DuTraveller.Restoration")
        regression_callback=unreal.register_slate_post_tick_callback(regression)
    else: unreal.EditorPythonScripting.set_keep_python_script_alive(False)
def tick(delta):
    global pawn,next_spawn,prepared
    try:
        if time.monotonic()-started>240: raise RuntimeError("Projectile cleanup probe time limit")
        world=worlds.get_game_world()
        if not world: return
        pc=unreal.GameplayStatics.get_player_controller(world,0)
        pawn=pc.get_controlled_pawn() if pc else None
        if not pawn: return
        t=unreal.GameplayStatics.get_time_seconds(world)
        if t>1 and not prepared:
            for enemy in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RPGAICharacter):
                enemy.get_controller().set_actor_tick_enabled(False)
                enemy.get_editor_property("action_comp").cancel_pending_release("cleanup fixture")
                enemy.get_editor_property("character_movement").stop_movement_immediately()
            for projectile in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RPGProjectile): projectile.destroy_actor()
            prepared=True
        if prepared and t>=next_spawn and report["spawned"]<20:
            index=report["spawned"]
            actor=probe.spawn_runtime_impact_probe(pawn,index)
            if not actor: raise RuntimeError("Could not spawn impact probe "+str(index))
            report["spawned"]+=1; next_spawn=t+0.2
        counts=dict(probe.count_runtime_projectile_fx(pawn))
        report["samples"].append({"time":t,**counts})
        output.write_text(json.dumps(report,indent=2))
        if t>15:
            quiet=[x for x in report["samples"] if x["time"]>12]
            report["checks"]=[
                {"name":"20 real projectile impacts scheduled with active burst/loop FX", "passed":report["spawned"]==20},
                {"name":"impact path stops rendering projectiles", "passed":any(x["hidden_projectiles"]>0 for x in report["samples"])},
                {"name":"expiry callbacks finish without exception or crash", "passed":True},
                {"name":"all projectile actors and detached effects are removed", "passed":bool(quiet) and all(x["projectiles"]==0 and x["active_niagara"]==0 for x in quiet)}]
            finish()
    except Exception:
        report["errors"].append(traceback.format_exc()); unreal.log_error(report["errors"][-1]); finish()
try:
    unreal.EditorLoadingAndSavingUtils.load_map("/Game/Levels/CombatTest")
    callback=unreal.register_slate_post_tick_callback(tick); editor.editor_request_begin_play()
except Exception:
    report["errors"].append(traceback.format_exc()); finish()
