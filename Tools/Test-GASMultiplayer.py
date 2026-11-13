"""Two-world PIE verification for granted GAS specs and remote hotbar activation. Never saves assets."""
import json,time,traceback
from pathlib import Path
import unreal

editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
callback=None; started=time.monotonic(); phase="startup"; tick_active=False
checks=[]; client_pawn=client_ps=server_pawn=server_ps=None
server_sequence=0; server_mana=0.0; dispatched=0.0
server_target=None; rejection_sequence=0; rejection_mana=0.0
rejection_health=0.0; rejection_dispatched=0.0
ability_id="Abilities.Fire.FireBolt"

def check(name,condition):
    checks.append(dict(name=name,passed=bool(condition)))
    if not condition: raise AssertionError(name)

def finish(reason,passed=False):
    global phase
    if phase=="finished": return
    phase="finished"
    if callback is not None: unreal.unregister_slate_post_tick_callback(callback)
    report=dict(passed=passed,reason=reason,checks=checks)
    out=Path(unreal.Paths.project_saved_dir())/"Automation"/"GASMultiplayer.json"
    out.parent.mkdir(parents=True,exist_ok=True); out.write_text(json.dumps(report,indent=2))
    unreal.log("GAS_MULTIPLAYER "+json.dumps(report))
    editor.editor_request_end_play()
    unreal.DTStructRepairLibrary.set_multiplayer_test_settings(False)
    unreal.SystemLibrary.quit_editor()

def tick(delta):
    global phase,client_pawn,client_ps,server_pawn,server_ps,server_sequence,server_mana,dispatched,tick_active
    global server_target,rejection_sequence,rejection_mana,rejection_health,rejection_dispatched
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
                    client_pawn=pc.get_controlled_pawn(); client_ps=pc.get_editor_property("player_state")
            if not client_ps or not client_pawn: return
            pid=client_ps.get_editor_property("player_id")
            for world in worlds:
                for ps in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RPGPlayerState):
                    if ps.has_authority() and ps.get_editor_property("player_id")==pid:
                        server_ps=ps; server_pawn=ps.get_pawn()
            if not server_pawn: return
            check("Server prepares and grants FireBolt",unreal.DTStructRepairLibrary.run_foundation_client_test_action(server_pawn,15))
            phase="spec_replication"
        elif phase=="spec_replication":
            skills=client_ps.get_editor_property("skills")
            if not skills.get_editor_property("hotbar"): return
            if not unreal.DTStructRepairLibrary.has_granted_gameplay_ability(client_pawn,ability_id): return
            slot_zero=next((entry for entry in skills.get_editor_property("hotbar")
                            if entry.get_editor_property("slot")==0),None)
            if slot_zero is None: return
            slot_definition=slot_zero.get_editor_property("definition")
            if str(slot_definition.get_editor_property("ability_id"))!=ability_id: return
            check("Granted Gameplay Ability spec replicates to owning client",True)
            check("Owner-only hotbar assignment replicates",True)
            server_sequence=server_pawn.get_editor_property("action_comp").get_editor_property("committed_action").get_editor_property("sequence")
            server_mana=server_ps.get_attribute_value("Mana")
            # Stage 16 also resolves the replicated enemy target used as the
            # gameplay-event payload. Give that actor time to reach the client.
            if not unreal.DTStructRepairLibrary.run_foundation_client_test_action(client_pawn,16): return
            check("Remote client dispatches hotbar activation",True)
            check("Owning client immediately predicts the active Gameplay Ability",
                  unreal.DTStructRepairLibrary.is_gameplay_ability_active(client_pawn,ability_id))
            check("Owning client immediately predicts the cooldown Gameplay Effect",
                  client_pawn.get_editor_property("action_comp").get_remaining_cooldown(ability_id)>0)
            dispatched=time.monotonic(); phase="activation"
        elif phase=="activation":
            server_action=server_pawn.get_editor_property("action_comp").get_editor_property("committed_action")
            if server_action.get_editor_property("sequence")<=server_sequence: return
            if client_ps.get_attribute_value("Mana")>=server_mana: return
            if client_pawn.get_editor_property("action_comp").get_remaining_cooldown(ability_id)<=0: return
            check("Server activates the granted ability from the client request",str(server_action.get_editor_property("ability_id"))==ability_id)
            check("Predicted gameplay event transports the selected target to the server",
                  server_action.get_editor_property("target") is not None)
            check("CommitAbility cost replicates to the owning client",client_ps.get_attribute_value("Mana")<server_mana)
            check("Server and client converge on committed mana",abs(client_ps.get_attribute_value("Mana")-server_ps.get_attribute_value("Mana"))<0.01)
            check("Cooldown Gameplay Effect replicates to the owning client",client_pawn.get_editor_property("action_comp").get_remaining_cooldown(ability_id)>0)
            check("Server retains the granted spec",unreal.DTStructRepairLibrary.has_granted_gameplay_ability(server_pawn,ability_id))
            server_target=server_action.get_editor_property("target")
            phase="rejection_ready"
        elif phase=="rejection_ready":
            client_action=client_pawn.get_editor_property("action_comp")
            server_action_comp=server_pawn.get_editor_property("action_comp")
            if unreal.DTStructRepairLibrary.is_gameplay_ability_active(client_pawn,ability_id): return
            if unreal.DTStructRepairLibrary.is_gameplay_ability_active(server_pawn,ability_id): return
            if client_action.get_remaining_cooldown(ability_id)>0: return
            if server_action_comp.get_remaining_cooldown(ability_id)>0: return
            rejection_sequence=server_action_comp.get_editor_property("committed_action").get_editor_property("sequence")
            rejection_health=unreal.DTStructRepairLibrary.read_runtime_attribute(server_target,"Health")
            check("Server creates an authoritative friendly-target disagreement",
                  unreal.DTStructRepairLibrary.run_foundation_client_test_action(server_pawn,17))
            rejection_mana=server_ps.get_attribute_value("Mana")
            check("Client dispatches a cast from its stale enemy view",
                  unreal.DTStructRepairLibrary.run_foundation_client_test_action(client_pawn,16))
            check("Rejected cast initially enters predicted active state",
                  unreal.DTStructRepairLibrary.is_gameplay_ability_active(client_pawn,ability_id))
            check("Rejected cast initially predicts its cooldown",
                  client_action.get_remaining_cooldown(ability_id)>0)
            rejection_dispatched=time.monotonic(); phase="rejection"
        elif phase=="rejection":
            client_action=client_pawn.get_editor_property("action_comp")
            server_action_comp=server_pawn.get_editor_property("action_comp")
            if unreal.DTStructRepairLibrary.is_gameplay_ability_active(client_pawn,ability_id): return
            if client_action.get_remaining_cooldown(ability_id)>0: return
            if time.monotonic()-rejection_dispatched<0.1: return
            if abs(client_ps.get_attribute_value("Mana")-server_ps.get_attribute_value("Mana"))>=0.01: return
            check("Server rejection rolls back the predicted active ability",True)
            check("Server rejection rolls back the predicted cooldown",True)
            check("Rejected cast does not commit a server combat action",
                  server_action_comp.get_editor_property("committed_action").get_editor_property("sequence")==rejection_sequence)
            check("Rejected cast does not spend authoritative mana",
                  abs(server_ps.get_attribute_value("Mana")-rejection_mana)<0.01)
            check("Owning client reconciles to unchanged mana",
                  abs(client_ps.get_attribute_value("Mana")-rejection_mana)<0.01)
            check("Rejected cast applies no target damage",
                  abs(unreal.DTStructRepairLibrary.read_runtime_attribute(server_target,"Health")-rejection_health)<0.01)
            check("Rejected cast creates no authoritative cooldown",
                  server_action_comp.get_remaining_cooldown(ability_id)<=0)
            finish("Two-world predicted GAS success and authoritative rejection/rollback checks passed",True)
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
    phase="connect"; callback=unreal.register_slate_post_tick_callback(tick)
    editor.editor_request_begin_play()
except Exception:
    finish(traceback.format_exc())
