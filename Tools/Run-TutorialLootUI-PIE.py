"""Validate live Tutorial enemies, grounded loot, and real Slate menu input. Never saves assets."""
import json,time,traceback,shutil
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
root=Path(unreal.Paths.project_dir()); output=root/"Saved/Automation/TutorialLootUI-PIE.json"
report={"passed":False,"checks":[],"errors":[],"audit":{}}
worlds=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
probe=unreal.DTStructRepairLibrary
started=time.monotonic(); callback=regression_callback=None
pc=pawn=enemy=loot=panel=None; flags=set(); baseline={}; seen_gear=False
def valid(obj): return obj is not None and unreal.SystemLibrary.is_valid(obj)
def save(): output.write_text(json.dumps(report,indent=2,default=str))
def check(name,passed): report["checks"].append({"name":name,"passed":bool(passed)}); save()
def key(name):
    result=unreal.Key(); result.set_editor_property("key_name",name); return result
def press(name):
    probe.inject_runtime_input(pc,name,unreal.Vector(1,0,0)); probe.inject_runtime_input(pc,name,unreal.Vector())
def ui_key(name): return probe.send_runtime_ui_key(pc,key(name))
def sequence(): return pawn.get_editor_property("action_comp").get_editor_property("committed_action").sequence
def meshes(actor): return [c for c in actor.get_components_by_class(unreal.StaticMeshComponent) if c.get_attach_parent()==actor.get_editor_property("mesh")]
def items(): return len(pawn.get_editor_property("inventory_comp").get_editor_property("items"))
def finish():
    global regression_callback
    unreal.unregister_slate_post_tick_callback(callback)
    report["passed"]=not report["errors"] and bool(report["checks"]) and all(c["passed"] for c in report["checks"]); save()
    if report["passed"]:
        editor.editor_request_end_play()
        def regression(delta):
            if worlds.get_game_world(): return
            unreal.unregister_slate_post_tick_callback(regression_callback)
            unreal.SystemLibrary.execute_console_command(worlds.get_editor_world(),"Automation RunTests DuTraveller.Restoration")
        regression_callback=unreal.register_slate_post_tick_callback(regression)
    else: unreal.EditorPythonScripting.set_keep_python_script_alive(False)
def tick(delta):
    global pc,pawn,enemy,loot,panel,seen_gear
    try:
        if time.monotonic()-started>300: raise RuntimeError("Tutorial probe time limit")
        world=worlds.get_game_world()
        if not world:
            if "init" in flags: raise RuntimeError("PIE ended before the validation sequence finished")
            return
        t=unreal.GameplayStatics.get_time_seconds(world)
        pc=unreal.GameplayStatics.get_player_controller(world,0)
        pawn=pc.get_controlled_pawn() if pc else None
        if not pawn: return
        if t>1 and "init" not in flags:
            enemies=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RPGAICharacter)
            report["audit"]["runtime_enemies"]=[{"name":a.get_name(),"class":a.get_class().get_path_name(),"loot":str(a.get_editor_property("loot_item_class")),"gear":[m.get_name() for m in meshes(a)]} for a in enemies]
            for a in enemies:
                if a.get_controller(): a.get_controller().set_actor_tick_enabled(False)
                a.get_editor_property("action_comp").cancel_pending_release("Tutorial UI/loot fixture")
                a.get_editor_property("character_movement").stop_movement_immediately()
            enemy=next((a for a in enemies if "BP_CombatTest_Enemy" in a.get_class().get_name()),None)
            if not enemy: raise RuntimeError("Tutorial has no live BP_CombatTest_Enemy; no substitute was spawned")
            for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RPGProjectile): a.destroy_actor()
            baseline["enemy_gear"]=len(meshes(enemy)); baseline["player_gear"]=len(meshes(pawn))
            pc.open_ui_panel("Attributes"); panel=pc.get_editor_property("panels")["Attributes"]
            baseline.update(sequence=sequence(),location=pawn.get_actor_location(),rotation=pc.get_control_rotation(),strength=probe.read_runtime_attribute(pawn,"Strength"))
            report["audit"]["attribute_widgets"]=[{"name":w.get_name(),"class":w.get_class().get_name()} for w in probe.get_runtime_ui_widgets(panel)]
            check("Attributes opens with cursor and movement/look locked",pc.should_block_gameplay_input() and pc.is_move_input_ignored() and pc.is_look_input_ignored() and pc.get_editor_property("show_mouse_cursor"))
            flags.add("init")
        if "init" in flags and 2<t<4:
            probe.inject_runtime_input(pc,"IA_Move",unreal.Vector(1,0,0))
            probe.inject_runtime_input(pc,"IA_Look",unreal.Vector(1,1,0))
        if t>2 and "clicks" not in flags:
            for name in ("One","LeftMouseButton","RightMouseButton"): press(name)
            ui_key("One")
            probe.click_runtime_ui_widget(panel,key("LeftMouseButton"),unreal.Vector2D(.8,.5))
            probe.click_runtime_ui_widget(panel,key("RightMouseButton"),unreal.Vector2D(.8,.5))
            flags.add("clicks")
        if t>3 and "upgrade" not in flags:
            row=next(w for w in probe.get_runtime_ui_widgets(panel) if w.get_name()=="Row_Strength")
            button=next(w for w in probe.get_runtime_ui_widgets(row) if isinstance(w,unreal.Button))
            report["attribute_click_handled"]=probe.click_runtime_ui_widget(button,key("LeftMouseButton"),unreal.Vector2D(.5,.5))
            flags.add("upgrade")
        if t>4 and "closed" not in flags:
            check("Real Slate click on the Strength plus button upgrades GAS Strength",probe.read_runtime_attribute(pawn,"Strength")>baseline["strength"])
            check("UI clicks and hotkeys cannot commit combat",sequence()==baseline["sequence"])
            check("Enhanced movement injection cannot move the menu-open pawn",(pawn.get_actor_location()-baseline["location"]).length()<2)
            check("Enhanced look injection cannot rotate the menu-open camera",abs(pc.get_control_rotation().yaw-baseline["rotation"].yaw)<.1 and abs(pc.get_control_rotation().pitch-baseline["rotation"].pitch)<.1)
            ui_key("Escape")
            check("Slate Escape closes the panel and releases input locks",str(pc.get_editor_property("active_panel"))=="None" and not pc.is_move_input_ignored() and not pc.is_look_input_ignored())
            pc.open_ui_panel("Inventory"); flags.add("closed")
        if t>5 and "inventory_closed" not in flags:
            ui_key("I")
            check("Slate inventory hotkey closes UI-only Inventory",str(pc.get_editor_property("active_panel"))=="None")
            baseline["move_start"]=pawn.get_actor_location(); flags.add("inventory_closed")
        if 5<t<6.5: probe.inject_runtime_input(pc,"IA_Move",unreal.Vector(1,0,0))
        if t>7 and "kill" not in flags:
            check("Movement works after closing menus",(pawn.get_actor_location()-baseline["move_start"]).length()>30)
            pc.close_ui_panels()
            pawn.set_actor_location(enemy.get_actor_location()+unreal.Vector(-170,0,0),False,True)
            baseline["sequence_after_ui"]=sequence(); press("One")
            check("Combat can commit after closing menus",sequence()>baseline["sequence_after_ui"])
            enemy.receive_combat_damage(10000,pawn,"Test.TutorialDeathLoot")
            enemy.spawn_death_loot() # Public Blueprint event must not duplicate drops.
            flags.add("kill")
        if t>8 and "loot" not in flags:
            drops=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RPGPickupItem) if a.get_owner()==enemy]
            check("Live Tutorial enemy creates exactly one configured loot actor",len(drops)==1)
            if not drops: raise RuntimeError("No live-enemy death loot")
            loot=drops[0]
            check("Enemy weapon components are cleared on death",baseline["enemy_gear"]>0 and len(meshes(enemy))==0)
            origin,extent=loot.get_actor_bounds(False)
            measurement=dict(probe.measure_runtime_ground_clearance(loot))
            ground=measurement.get("ground")
            report["loot"]={"class":loot.get_class().get_path_name(),"item":str(loot.get_editor_property("item_class")),"quantity":loot.get_editor_property("quantity"),"bottom":origin.z-extent.z,"ground":ground,"location":str(loot.get_actor_location())}
            check("Loot mesh bottom rests two centimetres above the ground",abs(measurement.get("clearance",-999)-2)<1)
            flags.add("loot")
        if t>13 and "collect" not in flags:
            check("Loot survives corpse removal",not valid(enemy) and valid(loot))
            baseline["inventory"]=items()
            pawn.set_actor_location(loot.get_actor_location()+unreal.Vector(-140,0,88),False,True)
            flags.add("collect")
        if 13.5<t<15 and valid(loot):
            camera=pc.get_editor_property("player_camera_manager")
            origin,extent=loot.get_actor_bounds(False)
            pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_camera_location(),origin))
            press("E")
        if t>15 and "player_death" not in flags:
            check("E collects grounded death loot into inventory",not valid(loot) and items()>baseline["inventory"])
            pawn.receive_combat_damage(10000,None,"Test.EquipmentDeath")
            check("Player equipment visuals clear during ragdoll",len(meshes(pawn))==0)
            flags.add("player_death")
        if t>32 and "skills" not in flags:
            check("Player respawn recreates equipped weapon visuals",not pawn.get_editor_property("bIsDead") and len(meshes(pawn))==baseline["player_gear"] and baseline["player_gear"]>0)
            pc.open_ui_panel("Skills"); panel=pc.get_editor_property("panels")["Skills"]
            skills=pc.get_editor_property("player_state").get_editor_property("skills")
            definition=next(entry.definition for entry in skills.get_editor_property("hotbar") if entry.slot==2)
            tag=unreal.GameplayTag(); tag.import_text('(TagName="'+str(definition.get_editor_property("ability_id"))+'")')
            model=panel.get_editor_property("widget_controller"); model.spell_globe_selected(tag); model.equip_button_pressed()
            baseline["skill_definition"]=definition; baseline["skill_sequence"]=sequence()
            flags.add("skills")
        if t>33 and "skill_key" not in flags:
            ui_key("Zero")
            skills=pc.get_editor_property("player_state").get_editor_property("skills")
            check("UI-only Skills still assigns keyboard hotbar slots",any(e.slot==11 and e.definition==baseline["skill_definition"] for e in skills.get_editor_property("hotbar")))
            panel.get_editor_property("widget_controller").equip_button_pressed(); flags.add("skill_key")
        if t>34 and "skill_lmb" not in flags:
            probe.click_runtime_ui_widget(panel,key("LeftMouseButton"),unreal.Vector2D(.8,.5))
            skills=pc.get_editor_property("player_state").get_editor_property("skills")
            check("Skills still assigns LMB without attacking",any(e.slot==0 and e.definition==baseline["skill_definition"] for e in skills.get_editor_property("hotbar")) and sequence()==baseline["skill_sequence"])
            panel.get_editor_property("widget_controller").equip_button_pressed(); flags.add("skill_lmb")
        if t>35 and "skill_rmb" not in flags:
            probe.click_runtime_ui_widget(panel,key("RightMouseButton"),unreal.Vector2D(.8,.5))
            skills=pc.get_editor_property("player_state").get_editor_property("skills")
            check("Skills still assigns RMB without casting",any(e.slot==1 and e.definition==baseline["skill_definition"] for e in skills.get_editor_property("hotbar")) and sequence()==baseline["skill_sequence"])
            pc.close_ui_panels(); pc.open_ui_panel("Attributes"); panel=pc.get_editor_property("panels")["Attributes"]
            flags.add("skill_rmb")
        if t>36 and "exit_button" not in flags:
            exit_widget=next(w for w in probe.get_runtime_ui_widgets(panel) if w.get_name()=="WBP_ButtonExit")
            button=next(w for w in probe.get_runtime_ui_widgets(exit_widget) if isinstance(w,unreal.Button))
            probe.click_runtime_ui_widget(button,key("LeftMouseButton"),unreal.Vector2D(.5,.5)); flags.add("exit_button")
        if t>37:
            check("Retained Blueprint exit button closes UI and releases gameplay locks",str(pc.get_editor_property("active_panel"))=="None" and not pc.is_move_input_ignored() and not pc.is_look_input_ignored())
            finish()
    except Exception:
        report["errors"].append(traceback.format_exc()); unreal.log_error(report["errors"][-1]); finish()
try:
    health_path="/Game/UI/World/WBP_EnemyHealthWidget"
    backup=root/"Saved/RestorationBackups/TutorialLootUI/WBP_EnemyHealthWidget.uasset"
    if not backup.exists():
        backup.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(root/"Content/UI/World/WBP_EnemyHealthWidget.uasset",backup)
    if not probe.repair_unified_widget_context(health_path) or not probe.force_compile_and_save(health_path):
        raise RuntimeError("Enemy health widget shared-base repair did not compile")
    report["audit"]["health_widget_repaired"]=health_path
    unreal.EditorLoadingAndSavingUtils.load_map("/Game/Levels/Tutorial")
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    report["audit"]["saved_enemies"]=[{"label":a.get_actor_label(),"class":a.get_class().get_path_name(),"loot":str(a.get_editor_property("loot_item_class"))} for a in actors if isinstance(a,unreal.RPGAICharacter)]
    for path in ("/Game/UI/Attribute/WBP_AttributeMenu","/Game/UI/HUD/WBP_HUD","/Game/Blueprints/Framework/BP_PlayerController","/Game/UI/World/WBP_EnemyHealthWidget"):
        probe.dump_blueprint_graph(path)
    save(); callback=unreal.register_slate_post_tick_callback(tick); editor.editor_request_begin_play()
except Exception:
    report["errors"].append(traceback.format_exc()); save(); unreal.EditorPythonScripting.set_keep_python_script_alive(False)


