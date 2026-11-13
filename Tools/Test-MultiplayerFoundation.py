"""Two-world PIE regression. Uses transient fixtures and never saves assets/settings."""
import json
import time
import traceback
from pathlib import Path
import unreal

editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
callback = None
started = time.monotonic()
phase = "startup"
last_diagnostic = 0
tick_active = False
checks = []
ui_point_events = []
quest_events = []
quest_results = []
test_random_quests = "-TestRandomQuests" in unreal.SystemLibrary.get_command_line()
test_ai_combat = "-TestAICombat" in unreal.SystemLibrary.get_command_line()
test_party = "-TestParty" in unreal.SystemLibrary.get_command_line()
server_spawner = None
quest_replay_started = 0
quest_replication_diagnostic_logged = False
server_pawn = client_pawn = server_ps = client_ps = client_pc = server_host_ps = None
controller = gear = food = None
helmet_slot = next(getattr(unreal.EquipableSlot, n) for n in dir(unreal.EquipableSlot) if n.endswith("HELMET"))


def check(name, condition):
    checks.append(dict(name=name, passed=bool(condition)))
    if not condition:
        raise AssertionError(name)


def points_changed(value):
    ui_point_events.append(value)


def quest_changed():
    quest_events.append(1)


def quest_request_result(succeeded, operation):
    quest_results.append((bool(succeeded), str(operation)))


def finish(reason, passed=False):
    global phase
    if phase == "finished":
        return
    phase = "finished"
    if callback is not None:
        unreal.unregister_slate_post_tick_callback(callback)
    report = dict(passed=passed, reason=reason, checks=checks)
    path = Path(unreal.Paths.project_saved_dir()) / "Automation" / "MultiplayerFoundation.json"
    if test_random_quests:
        path = path.with_name("SpawningQuestsMultiplayer.json")
    elif test_ai_combat:
        path = path.with_name("AICombatMultiplayer.json")
    elif test_party:
        path = path.with_name("PartyMultiplayer.json")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("FOUNDATION_MULTIPLAYER " + json.dumps(report))
    editor.editor_request_end_play()
    unreal.DTStructRepairLibrary.set_multiplayer_test_settings(False)
    unreal.SystemLibrary.quit_editor()


def tick(delta):
    global phase, server_pawn, client_pawn, server_ps, client_ps, client_pc, server_host_ps, controller, gear, food, last_diagnostic, tick_active, server_spawner, quest_replay_started, quest_replication_diagnostic_logged
    if tick_active:
        return
    tick_active = True
    try:
        if time.monotonic() - last_diagnostic > 10:
            last_diagnostic = time.monotonic()
            state = dict(phase=phase)
            for label, ps, pawn in [("server", server_ps, server_pawn), ("client", client_ps, client_pawn)]:
                if ps and pawn:
                    state[label] = dict(points=ps.get_editor_property("attribute_points"),
                                        strength=ps.get_attribute_value("Strength"),
                                        health=ps.get_attribute_value("Health"),
                                        items=len(pawn.get_editor_property("inventory_comp").get_editor_property("items")))
            unreal.log("FOUNDATION_MULTIPLAYER_STATE " + json.dumps(state))
        if time.monotonic() - started > 100:
            finish("Timeout during " + phase)
            return
        worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
        if phase == "connect":
            if len(worlds) < 2:
                return
            for world in worlds:
                pc = unreal.GameplayStatics.get_player_controller(world, 0)
                if pc and pc.get_controlled_pawn() and not pc.has_authority():
                    client_pc = pc
                    client_pawn = pc.get_controlled_pawn()
                    client_ps = pc.get_editor_property("player_state")
            if not client_ps:
                return
            player_id = client_ps.get_editor_property("player_id")
            for world in worlds:
                for ps in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.RPGPlayerState):
                    if ps.has_authority() and ps.get_editor_property("player_id") == player_id:
                        server_ps = ps
                        server_pawn = ps.get_pawn()
            if not server_pawn:
                return
            for ps in unreal.GameplayStatics.get_all_actors_of_class(server_pawn, unreal.RPGPlayerState):
                if ps != server_ps and ps.get_pawn():
                    server_host_ps = ps
                    ps.grant_progression_points(9, 7)
                    ps.get_pawn().get_editor_property("inventory_comp").try_add_item_from_class(unreal.Currency_ItemObject, 99)
                    ps.get_pawn().get_editor_property("character_movement").set_editor_property("gravity_scale", 0)
            server_pawn.get_editor_property("character_movement").set_editor_property("gravity_scale", 0)
            check("Server grants points", server_ps.grant_progression_points(2, 3))
            inv = server_pawn.get_editor_property("inventory_comp")
            check("Server prepares temporary item definitions", unreal.DTStructRepairLibrary.prepare_foundation_test_inventory(server_pawn))
            check("Fixture gear has configured slot", inv.get_item(0).get_editor_property("slot") == helmet_slot)
            bonuses = {str(key): float(value) for key, value in inv.get_item(0).get_editor_property("attribute_bonuses").items()}
            check("Fixture gear has configured bonuses", bonuses.get("Strength") == 5.0)
            phase = "initial_replication"
        elif phase == "initial_replication":
            inv = client_pawn.get_editor_property("inventory_comp")
            if client_ps.get_editor_property("attribute_points") != 2 or len(inv.get_editor_property("items")) != 2:
                return
            hud = client_pc.get_editor_property("hud_widget") if client_pc else None
            if not hud:
                return
            check("Owner receives spell points", client_ps.get_editor_property("spell_points") == 3)
            for pawn in unreal.GameplayStatics.get_all_actors_of_class(client_pawn, unreal.RPGCharacter):
                if pawn != client_pawn:
                    check("Other player's inventory is private", len(pawn.get_editor_property("inventory_comp").get_editor_property("items")) == 0)
                    check("Other player's unspent points are private", pawn.get_editor_property("player_state").get_editor_property("attribute_points") == 0)
            gear = inv.get_item(0)
            food = inv.get_item(1)
            if not gear or not food:
                return
            controller = unreal.RPG_AbilitySystemLibrary.get_attribute_widget_controller(client_pawn)
            check("Attribute UI controller resolves", controller is not None)
            check("Production player controller creates HUD root", hud is not None)
            check("HUD root receives overlay controller",
                  hud.get_editor_property("widget_controller") == unreal.RPG_AbilitySystemLibrary.get_overlay_widget_controller(client_pawn))
            client_pc.toggle_inventory()
            inventory_widget = client_pc.get_editor_property("player_inventory_widget")
            check("Production inventory opens on owning client", inventory_widget is not None and inventory_widget.get_visibility() == unreal.SlateVisibility.VISIBLE)
            client_pc.toggle_inventory()
            check("Production inventory closes locally", inventory_widget.get_visibility() == unreal.SlateVisibility.COLLAPSED)
            check("Owning client item widget attaches populated hover tooltip",
                  unreal.DTStructRepairLibrary.run_foundation_client_test_action(client_pawn, 5))
            controller.get_editor_property("attribute_points_changed_delegate").add_callable(points_changed)
            check("Gameplay request dispatched from client", unreal.DTStructRepairLibrary.run_foundation_client_test_action(client_pawn, 0))
            phase = "equip_and_spend"
        elif phase == "equip_and_spend":
            if abs(client_ps.get_attribute_value("Strength") - 6.0) > 0.01:
                return
            check("Server validates and applies remote investment and gear", abs(server_ps.get_attribute_value("Strength") - 6.0) < 0.01)
            check("Permanent base remains separate", abs(client_ps.get_attribute_value("Strength", True) - 1.0) < 0.01)
            check("One attribute point consumed", client_ps.get_editor_property("attribute_points") == 1)
            check("Attribute UI receives replicated point change", 1 in ui_point_events)
            check("Consumable request dispatched from client", unreal.DTStructRepairLibrary.run_foundation_client_test_action(client_pawn, 1))
            phase = "consume"
        elif phase == "consume":
            if abs(client_ps.get_attribute_value("Health") - 125.0) > 0.01:
                return
            if len(client_pawn.get_editor_property("inventory_comp").get_editor_property("items")) != 1:
                return
            check("Remote consumable applies on server", abs(server_ps.get_attribute_value("Health") - 125.0) < 0.01)
            check("Consumed item removal replicates", True)
            check("Unequip request dispatched from client", unreal.DTStructRepairLibrary.run_foundation_client_test_action(client_pawn, 2))
            phase = "unequip"
        elif phase == "unequip":
            if abs(client_ps.get_attribute_value("Strength") - 1.0) > 0.01:
                return
            check("Unequip preserves invested base", abs(server_ps.get_attribute_value("Strength") - 1.0) < 0.01)
            check("Health clamps after gear removal", abs(client_ps.get_attribute_value("Health") - 100.0) < 0.01)
            check("Remote drop request dispatched", unreal.DTStructRepairLibrary.run_foundation_client_test_action(client_pawn, 6))
            phase = "drop"
        elif phase == "drop":
            pickups = unreal.GameplayStatics.get_all_actors_of_class(client_pawn, unreal.RPGPickupItem)
            if client_pawn.get_editor_property("inventory_comp").get_editor_property("items") or not pickups:
                return
            pickup = pickups[0]
            if not pickup.get_editor_property("item_class") or pickup.get_editor_property("quantity") != 1:
                return
            check("Dropped pickup class and quantity replicate", pickup.get_editor_property("item") is not None)
            check("Remote collection request dispatched", unreal.DTStructRepairLibrary.run_foundation_client_test_action(client_pawn, 7))
            phase = "pickup"
        elif phase == "pickup":
            if len(client_pawn.get_editor_property("inventory_comp").get_editor_property("items")) != 1:
                return
            check("Collected item returns to owning inventory", len(server_pawn.get_editor_property("inventory_comp").get_editor_property("items")) == 1)
            if test_random_quests:
                check("Server creates location quest spawner with authored assets", unreal.DTStructRepairLibrary.run_foundation_client_test_action(server_pawn, 10))
                server_spawner = unreal.GameplayStatics.get_all_actors_of_class(server_pawn, unreal.RPGRandomQuestSpawner)[0]
                phase = "quest_offers"
            elif test_ai_combat:
                check("Server spawns configured production enemy", unreal.DTStructRepairLibrary.run_foundation_client_test_action(server_pawn, 11))
                phase = "ai_combat"
            elif test_party:
                check("Authoritative host invites remote player", server_host_ps is not None and server_host_ps.get_editor_property("party").request_invite(server_ps) is None)
                phase = "party_invite"
            else:
                finish("Two-world gameplay checks passed", True)
        elif phase == "ai_combat":
            enemies = unreal.GameplayStatics.get_all_actors_of_class(client_pawn, unreal.RPGAICharacter)
            if not enemies or client_ps.get_attribute_value("Health") >= 100.0:
                return
            action = enemies[0].get_editor_property("action_comp").get_editor_property("committed_action")
            if action.get_editor_property("sequence") < 1:
                return
            check("Production enemy damage replicates to owning client", client_ps.get_attribute_value("Health") < 100.0)
            check("Enemy committed-action presentation replicates", str(action.get_editor_property("ability_id")) == "AI.BasicAttack")
            check("Replicated enemy retains hostile faction", str(enemies[0].get_editor_property("faction_id")) == "Enemy")
            finish("Two-world production enemy combat checks passed", True)
        elif phase == "party_invite":
            party = client_ps.get_editor_property("party")
            if party.get_editor_property("pending_inviter") is None:
                return
            check("Pending invitation replicates owner-only", party.get_editor_property("pending_inviter") is not None)
            client_pc.toggle_party()
            widget = client_pc.get_editor_property("party_widget")
            check("Owning client opens production party panel", widget is not None and widget.get_visibility() == unreal.SlateVisibility.VISIBLE)
            check("Party panel receives owning party component", widget.get_editor_property("party") == party)
            check("Remote client submits invite acceptance", unreal.DTStructRepairLibrary.run_foundation_client_test_action(client_pawn, 13))
            phase = "party_accept"
        elif phase == "party_accept":
            party = client_ps.get_editor_property("party")
            if len(party.get_editor_property("members")) != 2:
                return
            server_party = server_ps.get_editor_property("party")
            check("Accepted roster converges on server and client", len(server_party.get_editor_property("members")) == 2)
            check("Party ID converges", party.get_editor_property("party_id").export_text() == server_party.get_editor_property("party_id").export_text())
            check("Server shares full XP through party roster", server_host_ps.grant_experience(17.0))
            phase = "party_xp"
        elif phase == "party_xp":
            if abs(client_ps.get_editor_property("exp") - 17.0) > 0.01:
                return
            check("Remote party member receives same XP amount", abs(server_ps.get_editor_property("exp") - 17.0) < 0.01)
            client_pc.toggle_party()
            check("Party panel closes locally", client_pc.get_editor_property("party_widget").get_visibility() == unreal.SlateVisibility.COLLAPSED)
            finish("Two-world party UI, membership and XP checks passed", True)
        elif phase == "quest_offers":
            spawners = unreal.GameplayStatics.get_all_actors_of_class(client_pawn, unreal.RPGRandomQuestSpawner)
            if not spawners or not spawners[0].get_editor_property("offers"):
                return
            check("Client cannot generate authoritative quest offers", not spawners[0].generate_offer())
            check("Client cannot spawn gameplay actors", spawners[0].try_spawn_one(spawners[0].get_editor_property("offers")[0].get_editor_property("offer_id")) is None)
            quests = client_ps.get_editor_property("dynamic_quests")
            quests.get_editor_property("on_quests_changed").add_callable(quest_changed)
            quests.get_editor_property("on_request_result").add_callable(quest_request_result)
            check("Client cannot forge objective progress", not quests.record_objective_event("Interacted", None, 100))
            check("Client submits quest acceptance", unreal.DTStructRepairLibrary.run_foundation_client_test_action(client_pawn, 3))
            phase = "quest_accept"
        elif phase == "quest_accept":
            quests = client_ps.get_editor_property("dynamic_quests")
            records = quests.get_editor_property("records")
            if not records:
                return
            check("Accepted quest reaches owner", len(records) == 1)
            check("Quest UI notification fires", bool(quest_events))
            instance_id = records[0].get_editor_property("instance_id")
            offer_id = records[0].get_editor_property("source_offer_id")
            for ps in unreal.GameplayStatics.get_all_actors_of_class(server_pawn, unreal.RPGPlayerState):
                if ps != server_ps and ps.get_pawn():
                    check("Host also holds a private quest", ps.get_editor_property("dynamic_quests").try_accept_offer(server_spawner, offer_id))
            check("Accept replay dispatched", unreal.DTStructRepairLibrary.run_foundation_client_test_action(client_pawn, 3))
            target = server_spawner.try_spawn_one(instance_id)
            check("Server spawns linked quest objective", target is not None)
            link = target.get_component_by_class(unreal.RPGSpawnContextComponent)
            check("Server interaction completes objective", link.report_quest_event("Interacted", 1))
            check("Duplicate interaction cannot add credit", not link.report_quest_event("Interacted", 1))
            phase = "quest_complete"
        elif phase == "quest_complete":
            records = client_ps.get_editor_property("dynamic_quests").get_editor_property("records")
            if not records or "COMPLETE" not in str(records[0].get_editor_property("status")):
                return
            linked = []
            pickups = unreal.GameplayStatics.get_all_actors_of_class(client_pawn, unreal.RPGPickupItem)
            expected_guid_text = records[0].get_editor_property("instance_id").export_text()
            for actor in pickups:
                context = actor.get_component_by_class(unreal.RPGSpawnContextComponent)
                if context and context.get_editor_property("quest_instance_id").export_text() == expected_guid_text:
                    linked.append(context)
            if not linked:
                if not quest_replication_diagnostic_logged:
                    quest_replication_diagnostic_logged = True
                    actor_details = []
                    for actor in pickups:
                        context = actor.get_component_by_class(unreal.RPGSpawnContextComponent)
                        actor_details.append({
                            "name": actor.get_name(),
                            "class": actor.get_class().get_path_name(),
                            "context": bool(context),
                            "quest": context.get_editor_property("quest_instance_id").export_text() if context else None,
                            "spawn": context.get_editor_property("spawn_id").export_text() if context else None,
                            "location": str(context.get_editor_property("location_id")) if context else None,
                        })
                    unreal.log_warning("QUEST_REPLICATION_DIAGNOSTIC " + json.dumps({
                        "expected": expected_guid_text,
                        "pickup_count": len(pickups),
                        "pickups": actor_details,
                        "spawner_alive_counts": [len(s.get_editor_property("alive_actors")) for s in unreal.GameplayStatics.get_all_actors_of_class(client_pawn, unreal.RPGRandomQuestSpawner)],
                    }))
                return
            check("Quest actor and context replicate to client", True)
            check("Spawn location metadata replicates", str(linked[0].get_editor_property("location_id")) == "Location.Ruins")
            check("Client actor event cannot forge server progress", not linked[0].report_quest_event("Interacted", 1))
            check("Acceptance replay leaves one quest", len(records) == 1)
            for ps in unreal.GameplayStatics.get_all_actors_of_class(client_pawn, unreal.RPGPlayerState):
                if ps != client_ps:
                    check("Other player's quest records remain private", len(ps.get_editor_property("dynamic_quests").get_editor_property("records")) == 0)
            check("Client submits reward claim", unreal.DTStructRepairLibrary.run_foundation_client_test_action(client_pawn, 4))
            phase = "quest_claim"
        elif phase == "quest_claim":
            quests = client_ps.get_editor_property("dynamic_quests")
            records = quests.get_editor_property("records")
            if "CLAIMED" not in str(records[0].get_editor_property("status")):
                return
            inv = client_pawn.get_editor_property("inventory_comp")
            if inv.count_items_of_exact_class(unreal.Currency_ItemObject) != 10:
                return
            rewards = records[0].get_editor_property("rolled_rewards")
            for reward in rewards:
                if inv.count_items_of_exact_class(reward.get_editor_property("item_class")) != reward.get_editor_property("quantity"):
                    return
            check("Selected random reward and quantity reach owner", True)
            check("Currency reward replicates once", True)
            check("XP reward replicates", abs(client_ps.get_editor_property("exp") - 25.0) < 0.01)
            check("Reward replay dispatched", unreal.DTStructRepairLibrary.run_foundation_client_test_action(client_pawn, 4))
            quest_replay_started = time.monotonic()
            phase = "quest_replay"
        elif phase == "quest_replay" and time.monotonic() - quest_replay_started > 1:
            inv = server_pawn.get_editor_property("inventory_comp")
            check("Repeated reward claim cannot duplicate money", inv.count_items_of_exact_class(unreal.Currency_ItemObject) == 10)
            check("Repeated reward claim cannot duplicate XP", abs(server_ps.get_editor_property("exp") - 25.0) < 0.01)
            check("Owner receives rejected claim result", (False, "Claim") in quest_results)
            finish("Two-world foundation and random quest checks passed", True)
    except Exception:
        finish(traceback.format_exc())
    finally:
        tick_active = False


try:
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    # A saved package gives both PIE worlds a stable level identity for RPCs.
    # The game-mode override below is deliberately never saved to the map.
    unreal.EditorLoadingAndSavingUtils.load_map("/Game/Levels/MainMenu")
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", unreal.RPGGameModeBase)
    unreal.DTStructRepairLibrary.set_multiplayer_test_settings(True)
    phase = "connect"
    callback = unreal.register_slate_post_tick_callback(tick)
    editor.editor_request_begin_play()
except Exception:
    finish(traceback.format_exc())
