import json,time,traceback
from pathlib import Path
import unreal
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
root=Path(unreal.Paths.project_dir()); out=root/"Saved/Automation/CombatEncounterPIE.json"
report={"passed":False,"samples":[],"checks":[],"errors":[]}
worlds=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
probe=unreal.DTStructRepairLibrary
started=time.monotonic(); controller=pawn=enemy=pickup=None
last=-1; first_enemy=None; cast_at=0; cast_count=0; baseline=0; collected=False; killed=False; picked_after=False; death_time=None
callback=None
regression_callback=None
def save(): out.write_text(json.dumps(report,indent=2,default=str),encoding="utf-8")
def dead(actor): return bool(actor.get_editor_property("bIsDead"))

def valid(actor):
    # UE can retain a Python wrapper after native corpse/pickup destruction.
    try: return actor is not None and unreal.SystemLibrary.is_valid(actor)
    except TypeError: return False
def check(name,ok): report["checks"].append({"name":name,"passed":bool(ok)})
def finish():
    global regression_callback
    if callback is not None: unreal.unregister_slate_post_tick_callback(callback)
    report["passed"]=not report["errors"] and bool(report["checks"]) and all(c["passed"] for c in report["checks"])
    save()
    if globals().get("RUN_REGRESSION_AFTER",False):
        editor.editor_request_end_play()
        def run_regression(delta):
            if worlds.get_game_world(): return
            unreal.unregister_slate_post_tick_callback(regression_callback)
            unreal.SystemLibrary.execute_console_command(worlds.get_editor_world(),"Automation RunTests DuTraveller.Restoration")
        regression_callback=unreal.register_slate_post_tick_callback(run_regression)
    else:
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
def sample(world,t):
    row={"time":t,"player_health":probe.read_runtime_attribute(pawn,"Health"),"pickups":len(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RPGPickupItem)),"enemy_count":len(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RPGAICharacter)),"projectiles":[]}
    row["player_runtime"]=dict(probe.inspect_runtime_player(controller))
    row["spawners"]=[{"area":str(a.get_editor_property("area_id")),"alive":len(a.get_editor_property("alive_actors")),"cap":a.get_editor_property("maximum_alive")} for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RPGSpawnArea)]
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RPGProjectile): row["projectiles"].append(str(actor.get_editor_property("ability_id")))
    if valid(enemy):
        mesh=enemy.get_editor_property("mesh"); anim=mesh.get_anim_instance()
        hand=mesh.get_socket_transform("hand_r",unreal.RelativeTransformSpace.RTS_COMPONENT).translation
        row.update(enemy_health=probe.read_runtime_attribute(enemy,"Health"),enemy_speed=enemy.get_velocity().length(),enemy_dead=dead(enemy),anim_class=anim.get_class().get_path_name() if anim else None,ground_speed=anim.get_editor_property("GroundSpeed") if anim else None,hand=[hand.x,hand.y,hand.z])
        try:
            montage=anim.get_current_active_montage() if anim else None
            row["montage"]=montage.get_path_name() if montage else None
        except Exception as exc: row["montage_probe_error"]=str(exc)
    report["samples"].append(row); save()
def tick(delta):
    global controller,pawn,enemy,pickup,last,first_enemy,cast_at,cast_count,baseline,collected,killed,picked_after,death_time
    try:
        if time.monotonic()-started>240: raise RuntimeError("Combat trace time limit")
        world=worlds.get_game_world()
        if not world: return
        t=unreal.GameplayStatics.get_time_seconds(world)
        if pawn is None:
            controller=unreal.GameplayStatics.get_player_controller(world,0)
            pawn=controller.get_controlled_pawn() if controller else None
            if not pawn: return
            baseline=len(pawn.get_editor_property("inventory_comp").get_editor_property("items"))
        live=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RPGAICharacter) if not dead(a)]
        if enemy is None and live: enemy=live[0]; first_enemy=enemy.get_name()
        if t-last>0.2: sample(world,t); last=t
        if t>4 and not collected:
            pickups=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RPGPickupItem)
            if pickup is None and pickups:
                pickup=pickups[0]
                pawn.set_actor_location(pickup.get_actor_location()+unreal.Vector(-140,0,88),False,True)
            if valid(pickup):
                camera=controller.get_editor_property("player_camera_manager")
                controller.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_camera_location(),pickup.get_actor_location()))
                probe.inject_runtime_input(controller,"E",unreal.Vector(1,0,0)); probe.inject_runtime_input(controller,"E",unreal.Vector(0,0,0))
            elif pickup:
                collected=True
                report["pickup_inventory_count"]=len(pawn.get_editor_property("inventory_comp").get_editor_property("items"))
        if t>6 and collected and valid(enemy) and not dead(enemy):
            if controller.get_editor_property("locked_combat_target")!=enemy:
                controller.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(pawn.get_actor_location(),enemy.get_actor_location()))
                controller.toggle_target_lock()
            if t>7 and t>=cast_at and controller.get_editor_property("locked_combat_target")==enemy:
                # First cast is attempted under attack. Later casts are staged at
                # range so interruption recovery and a completed notify release
                # are both exercised; no damage or release events are injected.
                if t>10:
                    pawn.set_actor_location(enemy.get_actor_location()+unreal.Vector(650 if cast_count%2 else -650,0,0),False,True)
                    controller.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(pawn.get_actor_location(),enemy.get_actor_location()))
                probe.inject_runtime_input(controller,"One",unreal.Vector(1,0,0)); probe.inject_runtime_input(controller,"One",unreal.Vector(0,0,0))
                cast_count+=1
                cast_at=t+3.0
        if valid(enemy) and dead(enemy) and not killed:
            killed=True; death_time=t
            report["death_pickup_count"]=len(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RPGPickupItem))
            report["enemy_owned_loot"]=len([a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RPGPickupItem) if a.get_owner()==enemy])
        if t>36:
            rows=report["samples"]
            check("enemy spawner creates an animated enemy",any(r.get("anim_class") for r in rows))
            check("enemy chases with movement",any(r.get("enemy_speed",0)>30 for r in rows))
            check("locomotion blendspace receives live speed",any(r.get("ground_speed",0)>30 for r in rows))
            hands={tuple(round(v,1) for v in r["hand"]) for r in rows if r.get("hand")}
            check("enemy skeletal pose is evaluated",len(hands)>5)
            check("enemy casting montage runs",any("Cast_Spell" in (r.get("montage") or "") for r in rows))
            check("enemy melee montage runs",any("Slash" in (r.get("montage") or "") for r in rows))
            check("enemy releases a spell projectile",any("AI.CombatTest.FireBolt" in r["projectiles"] for r in rows))
            check("enemy damages the player",any(r["player_health"]<100 for r in rows))
            check("item spawner produces pickups",any(r["pickups"]>0 for r in rows))
            check("E interaction collects an item into inventory",collected and report.get("pickup_inventory_count",0)>baseline)
            check("player spells kill the enemy",killed)
            check("enemy death creates loot",killed and report.get("enemy_owned_loot",0)>0)
            check("enemy spawner refills after corpse cleanup",killed and any(a.get_name()!=first_enemy for a in live))
            check("spawners respect alive caps",all(a["alive"]<=a["cap"] for r in rows for a in r["spawners"]))
            check("item spawner replenishes collected items",collected and any(a["area"]=="CombatTest_ItemSpawner" and a["alive"]==2 for r in rows for a in r["spawners"]))
            finish()
    except Exception:
        report["errors"].append(traceback.format_exc()); unreal.log_error(report["errors"][-1]); finish()
try:
    report["legacy_templates"]={}
    for path in ("/Game/Blueprints/Gameplay/Spawning/BP_MasterRandomSpawnArea","/Game/Blueprints/Actors/Interactables/Items/BP_MasterPickupItem","/Game/Blueprints/Actors/Interactables/Items/BP_Pickup"):
        asset=unreal.load_asset(path)
        defaults=unreal.get_default_object(asset.generated_class()) if asset else None
        info={"class":defaults.get_class().get_path_name() if defaults else None}
        if isinstance(defaults,unreal.RPGSpawnArea): info["pool"]=str(defaults.get_editor_property("spawn_pool"))
        if isinstance(defaults,unreal.RPGPickupItem): info["item_class"]=str(defaults.get_editor_property("item_class"))
        report["legacy_templates"][path]=info
    unreal.EditorLoadingAndSavingUtils.load_map("/Game/Levels/Combat_Test")
    callback=unreal.register_slate_post_tick_callback(tick); editor.editor_request_begin_play()
except Exception:
    report["errors"].append(traceback.format_exc()); finish()
