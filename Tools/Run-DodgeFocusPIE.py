"""Rendered Tutorial avoidance, pose-refresh and real montage-released threat validation."""
import json,time,traceback,math
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir()); out=root/"Saved/Automation/DodgeFocusPIE.json"
report={"passed":False,"checks":[],"samples":[],"rolls":[],"errors":[]}
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
worlds=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem); editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
probe=unreal.DTStructRepairLibrary; callback=regression_callback=None
started=time.monotonic(); clock=None; last_world_time=None; last_dilation=1.; pc=pawn=enemy=sword=None; flags=set(); rolls={}; last_sample=-1; previous_pose=None; stable=0; max_stable=0
focus_started=None; focus_ended=None; focus_roll_start=None; focus_health=None; next_stress_cast=20
directions=[(-1,0),(1,0),(0,1),(0,-1),(1,1),(1,-1),(-1,1),(-1,-1)]
slash=unreal.load_asset("/Game/Data/Abilities/Enemies/DA_CombatTest_Slash")
fire=unreal.load_asset("/Game/Data/Abilities/Enemies/DA_CombatTest_FireBolt")
def valid(obj): return obj is not None and unreal.SystemLibrary.is_valid(obj)
def save(): out.write_text(json.dumps(report,indent=2,default=str))
def check(name,ok): report["checks"].append({"name":name,"passed":bool(ok)}); save()
def attr(name): return probe.read_runtime_attribute(pawn,name)
def press(name,down=True): return probe.inject_runtime_input(pc,name,unreal.Vector(1 if down else 0,0,0))
def tap(name): press(name); press(name,False)
def vector(v): return [v.x,v.y,v.z]
def pose():
    mesh=pawn.get_editor_property("mesh")
    return tuple(round(x,2) for bone in ("head","foot_l","foot_r") for x in vector(mesh.get_socket_transform(bone,unreal.RelativeTransformSpace.RTS_COMPONENT).translation))
def aim_enemy(distance):
    enemy.set_actor_location(pawn.get_actor_location()+unreal.Vector(distance,0,0),False,True)
    enemy.set_actor_rotation(unreal.Rotator(pitch=0,yaw=180,roll=0),True)
def stop_enemy():
    enemy.get_editor_property("action_comp").cancel_pending_release("avoidance fixture")
    enemy.get_editor_property("animation_router").stop_active_montage(.05)
def finish():
    global regression_callback
    unreal.unregister_slate_post_tick_callback(callback)
    pawn.set_focus_held(False)
    pawn.get_editor_property("mesh").set_visibility(True,True)
    report["passed"]=not report["errors"] and all(c["passed"] for c in report["checks"]); save()
    if report["passed"]:
        editor.editor_request_end_play()
        def regression(delta):
            if worlds.get_game_world(): return
            unreal.unregister_slate_post_tick_callback(regression_callback)
            unreal.SystemLibrary.execute_console_command(worlds.get_editor_world(),"Automation RunTests DuTraveller.Restoration")
        regression_callback=unreal.register_slate_post_tick_callback(regression)
    else: unreal.EditorPythonScripting.set_keep_python_script_alive(False)
def tick(delta):
    global pc,pawn,enemy,sword,clock,last_world_time,last_dilation,last_sample,previous_pose,stable,max_stable,focus_started,focus_ended,focus_roll_start,focus_health,next_stress_cast
    try:
        if time.monotonic()-started>240: raise RuntimeError("Avoidance PIE time limit")
        world=worlds.get_game_world()
        if not world: return
        pc=unreal.GameplayStatics.get_player_controller(world,0); pawn=pc.get_controlled_pawn() if pc else None
        if not pawn or unreal.GameplayStatics.get_time_seconds(world)<1: return
        if clock is None:
            enemies=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RPGAICharacter)
            enemy=next((e for e in enemies if "CombatTest_Enemy" in e.get_class().get_name()),None)
            if not enemy: return
            for e in enemies:
                if e.get_controller(): e.get_controller().set_actor_tick_enabled(False)
                e.get_editor_property("action_comp").cancel_pending_release("avoidance fixture")
            stop_enemy()
            for shot in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RPGProjectile): shot.destroy_actor()
            aim_enemy(2500); pc.set_control_rotation(unreal.Rotator(pitch=0,yaw=0,roll=0)); clock=0.; last_world_time=unreal.GameplayStatics.get_time_seconds(world)
            sword=next(i for i in pawn.get_editor_property("inventory_comp").get_editor_property("items") if isinstance(i,unreal.Equipment_ItemObject) and i.get_editor_property("slot")==unreal.EquipableSlot.EIS_PRIMARY_WEAPON and i.is_equipped())
            check("Player pose always refreshes bones",pawn.get_editor_property("mesh").get_editor_property("visibility_based_anim_tick_option")==unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
        world_now=unreal.GameplayStatics.get_time_seconds(world)
        clock+=(world_now-last_world_time)/max(.05,last_dilation); last_world_time=world_now; last_dilation=unreal.GameplayStatics.get_global_time_dilation(world)
        t=clock
        for index,(x,y) in enumerate(directions):
            at=2+index*2
            if t>=at and index not in rolls:
                start=pawn.get_actor_location(); stamina=attr("Stamina")
                direction=unreal.Vector(x/math.sqrt(x*x+y*y),y/math.sqrt(x*x+y*y),0)
                if index==0: tap("LeftAlt"); accepted=pawn.get_editor_property("bIsDodging")
                else: accepted=pawn.request_dodge(direction)
                rolls[index]={"start":start,"direction":direction,"stamina":stamina,"started":t,"frames":[]}
                check("Roll "+str(index)+" starts and consumes GAS stamina",accepted and attr("Stamina")<=stamina-14.9)
                if index==0:
                    health=attr("Health")
                    check("Dodge state blocks damage for the full roll animation",not pawn.receive_combat_damage(25,enemy,"Test.DodgeInvulnerability") and attr("Health")==health)
                if index==0: check("Roll cannot be repeatedly activated mid-roll",not pawn.request_dodge(direction))
            if index in rolls and t<at+.9:
                mesh=pawn.get_editor_property("mesh"); anim=mesh.get_anim_instance()
                rolls[index]["frames"].append({"time":t,"montage":str(anim.get_current_active_montage()),"pose":pose()})
            if index in rolls and t>at+1.5 and "done_"+str(index) not in flags:
                row=rolls[index]; offset=pawn.get_actor_location()-row["start"]
                forward=offset.x*row["direction"].x+offset.y*row["direction"].y
                report["rolls"].append({"index":index,"direction":vector(row["direction"]),"distance_along_direction":forward,"frames":row["frames"]})
                check("Roll "+str(index)+" displaces and ends cleanly",forward>400 and not pawn.get_editor_property("bIsDodging"))
                check("Roll "+str(index)+" visibly evaluates the full-body clip",any("AM_DT_DodgeRoll" in f["montage"] for f in row["frames"]) and len({f["pose"] for f in row["frames"]})>3 and min(f["pose"][2] for f in row["frames"])<100)
                flags.add("done_"+str(index))
        if 16.35<t<16.6 and "roll_shot" not in flags:
            folder=root/"Saved/Automation/DodgeFocusFrames"; folder.mkdir(parents=True,exist_ok=True)
            unreal.SystemLibrary.execute_console_command(world,'Shot SHOWUI filename="'+str(folder/"Roll.png")+'"'); flags.add("roll_shot")
        if t>18.1 and "ui" not in flags:
            pc.open_ui_panel("Inventory"); before=attr("Stamina"); tap("LeftAlt"); press("Q")
            check("Menus block dodge and focus",not pawn.get_editor_property("bIsDodging") and not pawn.get_editor_property("focus_held") and attr("Stamina")==before)
            press("Q",False); pc.close_ui_panels(); flags.add("ui")
        if t>19 and "cost" not in flags:
            flags.add("cost")
            report["stamina_after_recovery"]=attr("Stamina"); check("GAS stamina regenerates after dodging",attr("Stamina")>70)
        if 20<t<30:
            probe.inject_runtime_input(pc,"IA_Move",unreal.Vector(1,0,0))
            if t>next_stress_cast: tap("One" if int(next_stress_cast)%4==0 else "LeftMouseButton"); next_stress_cast+=2
        if 21<t<22 and "unequip" not in flags: pawn.unequip_item(unreal.EquipableSlot.EIS_PRIMARY_WEAPON); flags.add("unequip")
        if 23<t<24 and "equip" not in flags: pawn.equip_item(sword,unreal.EquipableSlot.EIS_PRIMARY_WEAPON); flags.add("equip")
        if 25<t<26 and "hidden" not in flags: pawn.get_editor_property("mesh").set_visibility(False,True); flags.add("hidden")
        if t>26.5 and "shown" not in flags: pawn.get_editor_property("mesh").set_visibility(True,True); flags.add("shown")
        if 20<t<30 and pawn.get_velocity().length()>100:
            current=pose()
            if previous_pose is not None and max(abs(a-b) for a,b in zip(current,previous_pose))<.05: stable+=delta
            else: stable=0
            previous_pose=current; max_stable=max(max_stable,stable)
        if t>31 and "stress_done" not in flags:
            pawn.get_editor_property("action_comp").cancel_pending_release("stress done"); pawn.get_editor_property("animation_router").stop_active_montage(.05)
            report["max_frozen_pose_seconds_while_moving"]=max_stable
            check("No frozen pose while running, casting, changing weapons or hidden",max_stable<.45)
            flags.add("stress_done")
        if t>33 and "melee" not in flags:
            aim_enemy(100); press("Q"); report["melee_started"]=enemy.get_editor_property("action_comp").try_activate_ability(slash,1,pawn); flags.add("melee")
        if t>33.2 and "melee_checked" not in flags:
            check("Held focus predicts the authored melee collision window",report["melee_started"] and pawn.get_editor_property("threat_focus_active") and unreal.GameplayStatics.get_global_time_dilation(world)<.5)
            stop_enemy(); press("Q",False)
            check("Releasing Q immediately restores normal speed",unreal.GameplayStatics.get_global_time_dilation(world)==1)
            flags.add("melee_checked")
        if t>35 and "unfocused_shot" not in flags:
            aim_enemy(850); report["unfocused_shot_started"]=enemy.get_editor_property("action_comp").try_activate_ability(fire,1,pawn); flags.add("unfocused_shot")
        if 35<t<40 and valid(pawn.get_editor_property("incoming_threat")) and isinstance(pawn.get_editor_property("incoming_threat"),unreal.RPGProjectile) and "no_q" not in flags:
            check("Incoming projectile alone does not activate slow motion",not pawn.get_editor_property("threat_focus_active") and unreal.GameplayStatics.get_global_time_dilation(world)==1); flags.add("no_q")
        if t>41 and "focused_shot" not in flags:
            stop_enemy(); aim_enemy(850); press("Q"); report["focused_shot_started"]=enemy.get_editor_property("action_comp").try_activate_ability(fire,1,pawn); focus_health=attr("Health"); flags.add("focused_shot")
        if 41<t<46 and pawn.get_editor_property("threat_focus_active") and focus_started is None:
            focus_started=time.monotonic(); focus_roll_start=pawn.get_actor_location()
            check("Holding Q slows a real montage-released hostile projectile",report["focused_shot_started"] and isinstance(pawn.get_editor_property("incoming_threat"),unreal.RPGProjectile))
            check("Player retains responsive speed in focus",pawn.get_editor_property("custom_time_dilation")>=3.9)
            check("Player can roll sideways during focus",pawn.request_dodge(unreal.Vector(0,1,0)))
        if focus_started is not None and focus_ended is None and not pawn.get_editor_property("threat_focus_active"): focus_ended=time.monotonic()
        if t>46 and "focus_done" not in flags:
            press("Q",False)
            report["focus_duration_real"]=focus_ended-focus_started if focus_started is not None and focus_ended is not None else None
            check("Focus has a bounded real-time window and restores dilation",report["focus_duration_real"] is not None and report["focus_duration_real"]<.85 and unreal.GameplayStatics.get_global_time_dilation(world)==1 and pawn.get_editor_property("custom_time_dilation")==1)
            check("Side roll avoids the incoming spell",focus_started is not None and attr("Health")>=focus_health)
            check("Focus does not shrink roll distance",focus_roll_start is not None and pawn.get_actor_location().y-focus_roll_start.y>400)
            flags.add("focus_done")
        if t>48 and "menu_threat" not in flags:
            stop_enemy(); aim_enemy(100); press("Q"); enemy.get_editor_property("action_comp").try_activate_ability(slash,1,pawn); flags.add("menu_threat")
        if t>48.2 and "menu_focus" not in flags:
            check("Second melee threat enables focus",pawn.get_editor_property("threat_focus_active"))
            pc.open_ui_panel("Attributes")
            check("Opening UI restores global and player time immediately",not pawn.get_editor_property("threat_focus_active") and unreal.GameplayStatics.get_global_time_dilation(world)==1 and pawn.get_editor_property("custom_time_dilation")==1)
            stop_enemy(); press("Q",False); pc.close_ui_panels(); flags.add("menu_focus")
        if t>52 and "death_threat" not in flags:
            aim_enemy(100); press("Q"); enemy.get_editor_property("action_comp").try_activate_ability(slash,1,pawn); flags.add("death_threat")
        if t>52.2 and "death" not in flags:
            check("Focus is active before death cleanup",pawn.get_editor_property("threat_focus_active"))
            pawn.receive_combat_damage(10000,enemy,"Test.AvoidanceDeath")
            check("Death restores normal time even though actor tick stops",not pawn.get_editor_property("threat_focus_active") and unreal.GameplayStatics.get_global_time_dilation(world)==1)
            stop_enemy(); press("Q",False); flags.add("death")
        if t-last_sample>.1:
            report["samples"].append({"time":t,"location":vector(pawn.get_actor_location()),"speed":pawn.get_velocity().length(),"dodge":pawn.get_editor_property("bIsDodging"),"focus":pawn.get_editor_property("threat_focus_active"),"held":pawn.get_editor_property("focus_held"),"world_time":unreal.GameplayStatics.get_time_seconds(world),"threat":str(pawn.get_editor_property("incoming_threat")),"hit_seconds":pawn.get_editor_property("incoming_hit_seconds"),"world_dilation":unreal.GameplayStatics.get_global_time_dilation(world),"pawn_dilation":pawn.get_editor_property("custom_time_dilation"),"stamina":attr("Stamina"),"health":attr("Health"),"pose":pose()}); last_sample=t; save()
        if t>70:
            check("Player respawns with normal animation and full resources",not pawn.get_editor_property("bIsDead") and attr("Health")==100 and attr("Stamina")==100 and not pawn.get_editor_property("bIsDodging"))
            check("Projectile prediction without Q was observed","no_q" in flags)
            finish()
    except Exception:
        report["errors"].append(traceback.format_exc()); unreal.log_error(report["errors"][-1]); finish()
try:
    unreal.EditorLoadingAndSavingUtils.load_map("/Game/Levels/Tutorial")
    callback=unreal.register_slate_post_tick_callback(tick); editor.editor_request_begin_play()
except Exception:
    report["errors"].append(traceback.format_exc()); save(); unreal.EditorPythonScripting.set_keep_python_script_alive(False)






