import json,time,traceback
from pathlib import Path
import unreal
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
root=Path(unreal.Paths.project_dir()); out=root/"Saved/Automation/CombatPresentationPIE.json"
report={"passed":False,"checks":[],"samples":[],"errors":[]}
worlds=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
probe=unreal.DTStructRepairLibrary
started=time.monotonic(); callback=None; regression_callback=None
pawn=pc=enemy=None; last=-1; flags=set(); inventory_before=0
def valid(obj):
    try: return obj is not None and unreal.SystemLibrary.is_valid(obj)
    except TypeError: return False
def check(name,ok): report["checks"].append({"name":name,"passed":bool(ok)})
def press(key):
    probe.inject_runtime_input(pc,key,unreal.Vector(1,0,0)); probe.inject_runtime_input(pc,key,unreal.Vector(0,0,0))
def vec(v): return [round(v.x,2),round(v.y,2),round(v.z,2)]
def widget_state(widget):
    if not valid(widget): return None
    state={"class":widget.get_class().get_path_name(),"visible":widget.is_visible(),"viewport":widget.is_in_viewport()}
    try:
        state["health"]=widget.get_editor_property("displayed_health_percent")
        state["icons"]={key:widget.is_status_icon_visible(key) for key in ("Stunned","Burning","Poisoned","Frozen","Shielded")}
    except Exception as exc: state["probe_error"]=str(exc)
    try: state["countdown"]=str(widget.get_editor_property("Time_TXT").get_text())
    except Exception: pass
    return state
def pose(actor):
    mesh=actor.get_editor_property("mesh"); anim=mesh.get_anim_instance()
    return {"location":vec(actor.get_actor_location()),"actor_rotation":str(actor.get_actor_rotation()),"mesh_rotation":str(mesh.get_editor_property("relative_rotation")),"speed":actor.get_velocity().length(),"head":vec(mesh.get_socket_location("head")),"pelvis":vec(mesh.get_socket_location("pelvis")),"foot_l":vec(mesh.get_socket_location("foot_l")),"foot_r":vec(mesh.get_socket_location("foot_r")),"foot_component":vec(mesh.get_socket_transform("foot_l",unreal.RelativeTransformSpace.RTS_COMPONENT).translation),"physics":mesh.is_simulating_physics(),"montage":str(anim.get_current_active_montage()) if anim else None}
def sample(world,t):
    row={"time":t,"dead":pawn.get_editor_property("bIsDead"),"health":probe.read_runtime_attribute(pawn,"Health"),"player":pose(pawn),"fx":dict(probe.count_runtime_projectile_fx(pawn)),"death_ui":widget_state(pc.get_editor_property("death_widget")),"target_ui":widget_state(pc.get_editor_property("target_status_widget")),"inventory":len(pawn.get_editor_property("inventory_comp").get_editor_property("items"))}
    if valid(enemy): row.update(enemy=pose(enemy),enemy_health=probe.read_runtime_attribute(enemy,"Health"),enemy_ui=widget_state(enemy.get_editor_property("enemy_health_widget")))
    report["samples"].append(row); out.write_text(json.dumps(report,indent=2))
def finish():
    global regression_callback
    unreal.unregister_slate_post_tick_callback(callback)
    report["passed"]=not report["errors"] and bool(report["checks"]) and all(c["passed"] for c in report["checks"])
    out.write_text(json.dumps(report,indent=2))
    if globals().get("RUN_REGRESSION_AFTER",False) and report["passed"]:
        editor.editor_request_end_play()
        def regression(delta):
            if worlds.get_game_world(): return
            unreal.unregister_slate_post_tick_callback(regression_callback)
            unreal.SystemLibrary.execute_console_command(worlds.get_editor_world(),"Automation RunTests DuTraveller.Restoration")
        regression_callback=unreal.register_slate_post_tick_callback(regression)
    else: unreal.EditorPythonScripting.set_keep_python_script_alive(False)
def tick(delta):
    global pawn,pc,enemy,last,inventory_before
    try:
        if time.monotonic()-started>300: raise RuntimeError("PIE time limit")
        world=worlds.get_game_world()
        if not world: return
        t=unreal.GameplayStatics.get_time_seconds(world)
        for moment,label in ((2.8,"MovingCast"),(6.8,"EnemyStatus"),(16.0,"Death"),(26.0,"Respawn"),(44.0,"SecondDeath")):
            if t>moment and "shot_"+label not in flags:
                folder=root/"Saved/Automation/CombatPresentationFrames"; folder.mkdir(parents=True,exist_ok=True)
                unreal.SystemLibrary.execute_console_command(world,'Shot SHOWUI filename="'+str(folder/(label+".png"))+'"')
                flags.add("shot_"+label)
        if not valid(pawn):
            pc=unreal.GameplayStatics.get_player_controller(world,0); pawn=pc.get_controlled_pawn() if pc else None
            if not valid(pawn): return
            inventory_before=len(pawn.get_editor_property("inventory_comp").get_editor_property("items"))
        if enemy is None:
            enemies=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RPGAICharacter)
            if enemies: enemy=enemies[0]
        if valid(enemy) and "lock" not in flags and t>1:
            pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(pawn.get_actor_location(),enemy.get_actor_location()))
            pc.toggle_target_lock(); flags.add("lock")
        if 1<t<4 or 28<t<31: probe.inject_runtime_input(pc,"IA_Move",unreal.Vector(1,0,0))
        if t>2 and "cast" not in flags: press("One"); flags.add("cast")
        if t>5 and "root" not in flags and valid(enemy):
            root_ability=unreal.load_asset("/Game/Data/Abilities/Player/DA_Player_Root")
            report["root_activated"]=pawn.get_editor_property("action_comp").try_activate_ability(root_ability,1,enemy)
            flags.add("root")
        if t>8 and "pause_ai" not in flags and valid(enemy):
            enemy.get_controller().set_actor_tick_enabled(False)
            enemy.get_editor_property("action_comp").cancel_pending_release("presentation probe pause")
            enemy.get_editor_property("character_movement").stop_movement_immediately()
            flags.add("pause_ai")
        if t>10 and "fatal" not in flags:
            pawn.receive_combat_damage(10000.0,enemy,"Test.DeathPresentation")
            flags.add("fatal")
            press("One")
        if t>27 and not pawn.get_editor_property("bIsDead") and "recovered" not in flags:
            if valid(enemy):
                pawn.set_actor_location(enemy.get_actor_location()+unreal.Vector(-650,0,0),False,True)
                pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(pawn.get_actor_location(),enemy.get_actor_location()))
                pc.toggle_target_lock()
            flags.add("recovered")
        if t>28 and "recovered" in flags and "recast" not in flags: press("One"); flags.add("recast")
        if t>38 and "second_fatal" not in flags:
            pawn.receive_combat_damage(10000.0,enemy,"Test.RepeatDeathPresentation")
            flags.add("second_fatal")
        if t-last>0.1: sample(world,t); last=t
        if t>57:
            rows=report["samples"]
            moving_cast=[r for r in rows if r["player"]["speed"]>100 and "Cast_Spell" in (r["player"]["montage"] or "")]
            check("player moves while casting",len(moving_cast)>2)
            check("walking legs change during upper-body cast",len({tuple(r["player"]["foot_component"]) for r in moving_cast})>2)
            check("player stays upright during moving cast",bool(moving_cast) and all(r["player"]["head"][2]-r["player"]["pelvis"][2]>40 for r in moving_cast))
            walking=[r for r in rows if r.get("enemy",{}).get("speed",0)>30]
            check("enemy walks upright above the floor",bool(walking) and all(r["enemy"]["head"][2]>100 and min(r["enemy"]["foot_l"][2],r["enemy"]["foot_r"][2])>-15 for r in walking))
            check("enemy health overlay is visible",any(r.get("enemy_ui",{} ) and r["enemy_ui"]["visible"] for r in rows))
            check("enemy health overlay follows GAS damage",any(r.get("enemy_ui") and 0<r["enemy_ui"].get("health",1)<1 for r in rows))
            check("target status notification is visible",any(r.get("target_ui") and r["target_ui"]["visible"] for r in rows))
            check("control status icon appears",any(r.get("enemy_ui") and r["enemy_ui"].get("icons",{}).get("Stunned",False) for r in rows))
            quiet=[r for r in rows if 18<r["time"]<24]
            check("projectiles stop rendering on impact before removal",any(r["fx"]["hidden_projectiles"]>0 for r in rows))
            check("projectiles and detached effects clean up",bool(quiet) and all(r["fx"]["projectiles"]==0 and r["fx"]["active_niagara"]==0 for r in quiet))
            dead=[r for r in rows if r["dead"]]
            check("player enters death and ragdoll",bool(dead) and any(r["player"]["physics"] for r in dead))
            check("dead player physically collapses toward floor",any(12<r["time"]<24 and r["player"]["head"][2]<65 for r in dead))
            check("dead health stays zero",bool(dead) and all(r["health"]==0 for r in dead))
            check("death UI is visible",any(r.get("death_ui") and r["death_ui"]["visible"] for r in dead))
            check("respawn countdown updates",len({r["death_ui"].get("countdown") for r in dead if r.get("death_ui")})>3)
            revived=[r for r in rows if 25.5<r["time"]<27]
            check("player respawns with full health",bool(revived) and all(not r["dead"] and r["health"]==100 for r in revived))
            check("respawn clears death UI and ragdoll",bool(revived) and all(not r["player"]["physics"] and not r.get("death_ui") for r in revived))
            check("respawn preserves inventory",bool(revived) and all(r["inventory"]==inventory_before for r in revived))
            check("movement and casting work after respawn",any(r["time"]>28 and r["player"]["speed"]>100 and "Cast_Spell" in (r["player"]["montage"] or "") for r in rows))
            check("repeat death physically collapses",any(40<r["time"]<50 and r["dead"] and r["player"]["head"][2]<65 for r in rows))
            second_revived=[r for r in rows if 54<r["time"]<56]
            check("repeat respawn restores the player",bool(second_revived) and all(not r["dead"] and r["health"]==100 and not r["player"]["physics"] and not r.get("death_ui") and r["inventory"]==inventory_before for r in second_revived))
            finish()
    except Exception:
        report["errors"].append(traceback.format_exc()); unreal.log_error(report["errors"][-1]); finish()
try:
    unreal.EditorLoadingAndSavingUtils.load_map("/Game/Levels/CombatTest")
    callback=unreal.register_slate_post_tick_callback(tick); editor.editor_request_begin_play()
except Exception:
    report["errors"].append(traceback.format_exc()); finish()
