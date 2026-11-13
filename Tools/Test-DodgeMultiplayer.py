"""Two-world PIE dodge verification: client-initiated roll must displace the authoritative
capsule and replicate to the owning client without snapback. Never saves assets."""
import json,time,traceback
from pathlib import Path
import unreal
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
callback=None; started=time.monotonic(); phase="startup"; tick_active=False
checks=[]; client_pc=client_pawn=client_ps=server_ps=server_pawn=None
start_server=start_client=None; dodge_dispatched=0.0
def check(name,condition):
    checks.append(dict(name=name,passed=bool(condition)))
    if not condition: raise AssertionError(name)
def finish(reason,passed=False):
    global phase
    if phase=="finished": return
    phase="finished"
    if callback is not None: unreal.unregister_slate_post_tick_callback(callback)
    report=dict(passed=passed,reason=reason,checks=checks)
    out=Path(unreal.Paths.project_saved_dir())/"Automation"/"DodgeMultiplayer.json"
    out.parent.mkdir(parents=True,exist_ok=True); out.write_text(json.dumps(report,indent=2))
    unreal.log("DODGE_MULTIPLAYER "+json.dumps(report))
    editor.editor_request_end_play()
    unreal.DTStructRepairLibrary.set_multiplayer_test_settings(False)
    unreal.SystemLibrary.quit_editor()
def tick(delta):
    global phase,client_pc,client_pawn,client_ps,server_ps,server_pawn,start_server,start_client,dodge_dispatched,tick_active
    if tick_active: return
    tick_active=True
    try:
        if time.monotonic()-started>100: finish("Timeout during "+phase); return
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if phase=="connect":
            if len(worlds)<2: return
            for world in worlds:
                pc=unreal.GameplayStatics.get_player_controller(world,0)
                if pc and pc.get_controlled_pawn() and not pc.has_authority():
                    client_pc=pc; client_pawn=pc.get_controlled_pawn(); client_ps=pc.get_editor_property("player_state")
            if not client_ps or not client_pawn: return
            pid=client_ps.get_editor_property("player_id")
            for world in worlds:
                for ps in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RPGPlayerState):
                    if ps.has_authority() and ps.get_editor_property("player_id")==pid:
                        server_ps=ps; server_pawn=ps.get_pawn()
            if not server_pawn: return
            # Dodge requires ground contact; settle then verify the pawn is grounded.
            phase="settle"
        elif phase=="settle":
            if unreal.GameplayStatics.get_time_seconds(client_pawn)<1.5: return
            check("Client pawn is grounded for the roll",client_pawn.get_editor_property("character_movement").is_moving_on_ground())
            check("Authoritative pawn is grounded for the roll",server_pawn.get_editor_property("character_movement").is_moving_on_ground())
            # Clear any UI panel/input locks left by map HUD before issuing the dodge.
            try: client_pc.close_ui_panels()
            except Exception as e: unreal.log_warning("CloseUIPanels failed: "+str(e))
            client_pc.set_ignore_move_input(False); client_pc.set_ignore_look_input(False)
            unreal.log("CLIENT_DIAG dead=%s stunned=%s dodging=%s panel=%s move_ign=%s paused=%s ctrl=%s enable=%s stamina=%s"%(
                client_pawn.get_editor_property("bIsDead"),client_pawn.get_editor_property("bIsStunned"),
                client_pawn.get_editor_property("bIsDodging"),client_pc.get_editor_property("active_panel"),
                client_pc.is_move_input_ignored(),unreal.GameplayStatics.is_game_paused(client_pawn),type(client_pawn.get_controller()).__name__,
                client_pawn.get_editor_property("bEnableDodge"),
                unreal.DTStructRepairLibrary.read_runtime_attribute(client_pawn,"Stamina")))
            start_server=server_pawn.get_actor_location(); start_client=client_pawn.get_actor_location()
            check("Client dodge request dispatched through real RPC routing",
                  unreal.DTStructRepairLibrary.run_foundation_client_test_action(client_pawn,14))
            dodge_dispatched=time.monotonic(); phase="dodge"
        elif phase=="dodge":
            if time.monotonic()-dodge_dispatched<1.6: return
            s_off=server_pawn.get_actor_location()-start_server
            c_off=client_pawn.get_actor_location()-start_client
            check("Authoritative capsule advances with the roll",s_off.x>400 and abs(s_off.y)<80)
            check("Owning client capsule converges on the roll destination",c_off.x>400 and abs(c_off.y)<80)
            check("Roll ends cleanly on the server",not server_pawn.get_editor_property("bIsDodging"))
            check("Roll ends cleanly on the client",not client_pawn.get_editor_property("bIsDodging"))
            check("No snapback below the authored dodge distance",s_off.x>475 and c_off.x>475)
            finish("Two-world dodge roll checks passed",True)
    except Exception:
        finish(traceback.format_exc())
    finally:
        tick_active=False
try:
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    unreal.EditorLoadingAndSavingUtils.load_map("/Game/Levels/MainMenu")
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode",
        unreal.load_class(None,"/Game/Blueprints/Framework/BP_RPGGameModeBase.BP_RPGGameModeBase_C"))
    unreal.DTStructRepairLibrary.set_multiplayer_test_settings(True)
    phase="connect"
    callback=unreal.register_slate_post_tick_callback(tick)
    editor.editor_request_begin_play()
except Exception:
    finish(traceback.format_exc())
