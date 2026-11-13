"""Rendered Tutorial PIE trace; transient test actors/loadout, no asset saves."""
import json
import time
import traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
output = Path(unreal.Paths.project_saved_dir()) / "Automation/LivePIETraceMelee.json"
events = []
started = time.monotonic()
last_sample = -1
phase = "start"
phase_start = 0
pawn = None
controller = None
victim = None
callback = None
editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
worlds = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
probe = unreal.DTStructRepairLibrary

def log(kind, **values):
    row = dict(kind=kind, real_time=round(time.monotonic()-started,3), **values)
    events.append(row)
    unreal.log("LIVE_PIE " + json.dumps(row,default=str))
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps(events,indent=2,default=str),encoding="utf-8")

def snapshot(label,world):
    state = dict(probe.inspect_runtime_player(controller))
    state["game_time"] = unreal.GameplayStatics.get_time_seconds(world)
    state["location"] = str(pawn.get_actor_location())
    mesh = pawn.get_editor_property("mesh")
    state["hand_r"] = str(mesh.get_socket_transform("hand_r",unreal.RelativeTransformSpace.RTS_COMPONENT))
    state["foot_l"] = str(mesh.get_socket_transform("foot_l",unreal.RelativeTransformSpace.RTS_COMPONENT))
    state["projectiles"] = len(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RPGProjectile))
    state["hud_mirror_mana"] = pawn.get_editor_property("attribute_comp").get_editor_property("mana")
    state["equipment"]=[{"mesh":str(c.get_editor_property("static_mesh")),"location":str(c.get_world_location()),"collision":str(c.get_collision_enabled())} for c in pawn.get_components_by_class(unreal.StaticMeshComponent) if "/Game/Art/3D/Weapons/" in str(c.get_editor_property("static_mesh"))]
    if victim:
        state["test_npc_health"] = probe.read_runtime_attribute(victim,"Health")
    state["projectile_details"] = [{"class":a.get_class().get_path_name(),"ability":str(a.get_editor_property("ability_id")),"location":str(a.get_actor_location())} for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RPGProjectile)]
    state["pawn_movement"] = {}
    for name in ("WantsToWalk","WantsToSprint","WantsToStrafe","Gait","RotationMode"):
        try: state["pawn_movement"][name]=str(pawn.get_editor_property(name))
        except Exception: pass
    state["mana_widgets"]=[]
    mana_class=unreal.load_class(None,"/Game/UI/HUD/SpellGlobes/AttributeGlobes/WBP_ManaGlobe.WBP_ManaGlobe_C")
    for widget in unreal.WidgetLibrary.get_all_widgets_of_class(world,mana_class,False):
        row={"widget":widget.get_path_name()}
        for name in ("Mana","MaxMana","WidgetController"):
            try: row[name]=str(widget.get_editor_property(name))
            except Exception: pass
        try:
            row["percent"]=widget.get_editor_property("ProgressBar").get_editor_property("percent")
        except Exception as exc: row["percent_error"]=str(exc)
        state["mana_widgets"].append(row)
    log("sample",label=label,state=state)

def finish(reason):
    log("finished",reason=reason)
    if callback is not None: unreal.unregister_slate_post_tick_callback(callback)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)

def tick(delta):
    global phase,phase_start,pawn,controller,last_sample,victim
    try:
        if time.monotonic()-started > 240:
            finish("real-time limit"); return
        world = worlds.get_game_world()
        if not world: return
        t = unreal.GameplayStatics.get_time_seconds(world)
        if pawn is None:
            controller = unreal.GameplayStatics.get_player_controller(world,0)
            pawn = controller.get_controlled_pawn() if controller else None
            if pawn:
                log("runtime_identity",state=dict(probe.inspect_runtime_player(controller)))
                characters=[]
                for character in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RPGCharacter):
                    cm=character.get_editor_property("mesh")
                    ai=cm.get_anim_instance()
                    characters.append({"actor":character.get_path_name(),"class":character.get_class().get_path_name(),"anim_instance":ai.get_class().get_path_name() if ai else None})
                log("runtime_characters",characters=characters)
                skills = controller.get_editor_property("player_state").get_editor_property("skills")
                log("authored_runtime_hotbar",hotbar=str(skills.get_editor_property("hotbar")))
                phase="idle"; phase_start=t
            return
        elapsed=t-phase_start
        if t-last_sample >= 0.2:
            snapshot(phase,world); last_sample=t
        if phase=="idle" and elapsed>=2:
            phase="walk"; phase_start=t
        elif phase in ("walk","jog","sprint","jump"):
            probe.inject_runtime_input(controller,"IA_Move",unreal.Vector(0,1,0))
            probe.inject_runtime_input(controller,"IA_Walk",unreal.Vector(1 if phase in ("walk","jog") and elapsed<0.1 else 0,0,0))
            probe.inject_runtime_input(controller,"IA_Sprint",unreal.Vector(1 if phase=="sprint" else 0,0,0))
            probe.inject_runtime_input(controller,"IA_Jump",unreal.Vector(1 if phase=="jump" and elapsed<0.2 else 0,0,0))
            if elapsed>=2.5:
                if phase=="walk": phase="jog"
                elif phase=="jog": phase="sprint"
                elif phase=="sprint": phase="jump"
                else:
                    phase="cast"
                    victim=probe.spawn_runtime_combat_target(pawn,600)
                    hand=pawn.get_editor_property("mesh").get_socket_location("hand_r")
                    aim=unreal.MathLibrary.find_look_at_rotation(hand,victim.get_actor_location())
                    controller.set_control_rotation(aim)
                    log("hotbar_press",slot=2,key="1",mana=probe.read_runtime_attribute(pawn,"Mana"))
                    probe.inject_runtime_input(controller,"One",unreal.Vector(1,0,0))
                    probe.inject_runtime_input(controller,"One",unreal.Vector(0,0,0))
                phase_start=t
        elif phase=="cast" and elapsed>=4:
            phase="melee"; phase_start=t
            victim.set_actor_location(pawn.get_actor_location()+pawn.get_actor_forward_vector()*90,False,True)
            probe.inject_runtime_input(controller,"LeftMouseButton",unreal.Vector(1,0,0))
            probe.inject_runtime_input(controller,"LeftMouseButton",unreal.Vector(0,0,0))
        elif phase=="melee" and elapsed>=3:
            phase="regen"; phase_start=t
        elif phase=="melee" and 0.4<elapsed<0.65:
            equipment=[c for c in pawn.get_components_by_class(unreal.StaticMeshComponent) if "/Game/Art/3D/Weapons/" in str(c.get_editor_property("static_mesh"))]
            if equipment: victim.set_actor_location(equipment[0].get_world_location(),False,True)
        elif phase=="regen" and elapsed>=8:
            finish("trace complete")
    except Exception:
        log("exception",trace=traceback.format_exc()); finish("exception")

try:
    audit=Path(unreal.Paths.project_dir())/"Tools/Audit-MeleeAndGait.py"
    exec(compile(audit.read_text(encoding="utf-8"),str(audit),"exec"),{"__file__":str(audit)})
    world=worlds.get_editor_world()
    if not world or world.get_name()!="Tutorial": unreal.EditorLoadingAndSavingUtils.load_map("/Game/Levels/Tutorial")
    world=worlds.get_editor_world()
    mode=world.get_world_settings().get_editor_property("default_game_mode")
    unreal.SystemLibrary.execute_console_command(world,"log LogRPGCombat verbose")
    log("map",name=world.get_name(),override=mode.get_path_name() if mode else None)
    callback=unreal.register_slate_post_tick_callback(tick)
    editor.editor_request_begin_play()
except Exception:
    log("exception",trace=traceback.format_exc()); finish("startup exception")
