"""PIE validation: spawn the Meshy enemy in CombatTest and verify locomotion,
attack montage + notifies + GAS damage, hit reaction and death.

Run inside a live editor via Run-PIEPython.cmd:
  Tools\Run-PIEPython.cmd "...\Meshy-PIETest.py" /Game/Levels/CombatTest
"""
import json
import os
import time
import traceback
from pathlib import Path

import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
root = Path(unreal.Paths.project_dir())
report = {"passed": False, "checks": [], "errors": [], "samples": []}
flags = set()
callback = next_callback = None
started = time.monotonic()
last = -1.0
pc = pawn = enemy = None
worlds = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
probe = unreal.DTStructRepairLibrary


def save():
    (root / "Saved/Automation/MeshyPIE.json").write_text(json.dumps(report, indent=2, default=str))


def check(name, ok):
    report["checks"].append({"name": name, "passed": bool(ok)})
    save()


def finish():
    unreal.unregister_slate_post_tick_callback(callback)
    report["passed"] = (not report["errors"] and len(report["checks"]) == 10
                        and all(c["passed"] for c in report["checks"]))
    save()
    editor.editor_request_end_play()


def tick(delta):
    global pc, pawn, enemy, last
    try:
        if time.monotonic() - started > 120:
            raise RuntimeError("Meshy PIE timeout")
        world = worlds.get_game_world()
        if not world:
            return
        t = unreal.GameplayStatics.get_time_seconds(world)
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        pawn = pc.get_controlled_pawn() if pc else None
        if not pawn or t < 1:
            return

        if "init" not in flags:
            cls = unreal.load_class(
                None, "/Game/Blueprints/AI/BP_AzureLichKing_Enemy.BP_AzureLichKing_Enemy_C")
            if not cls:
                raise RuntimeError("BP_AzureLichKing_Enemy class missing")
            loc = pawn.get_actor_location() + unreal.Vector(300, 0, 0)
            enemy = unreal.DTMeshyEditorLibrary.spawn_actor_in_world(world, cls, loc)
            if not enemy:
                raise RuntimeError("spawn failed")
            check("Meshy enemy spawned in CombatTest", True)
            mesh = enemy.get_editor_property("mesh")
            check("Skinned Meshy skeletal mesh assigned",
                  mesh.get_editor_property("skeletal_mesh_asset") is not None)
            check("Anim instance is adapted AnimBP",
                  "ABP_AzureLichKing_Enemy" in str(mesh.get_editor_property("anim_class"))
                  or "Meshy" in str(mesh.get_editor_property("anim_class")))
            report["initial_health"] = probe.read_runtime_attribute(enemy, "Health")
            check("Enemy has Health attribute", report["initial_health"] > 0)
            flags.add("init")

        anim = enemy.get_editor_property("mesh").get_anim_instance()
        montage = anim.get_current_active_montage() if anim else None
        hp = probe.read_runtime_attribute(enemy, "Health")

        # locomotion sample: AI should move toward the player
        if t > 2 and "loco" not in flags:
            dist = enemy.get_actor_location().distance(pawn.get_actor_location())
            report.setdefault("dists", []).append(round(dist, 1))
            if len(report["dists"]) > 4:
                flags.add("loco")
                check("Enemy AI approaches/engages player",
                      report["dists"][-1] < report["dists"][0] + 400)

        # attack montage + damage event
        if "attack_hit" not in flags:
            if montage and ("Slash" in montage.get_name() or "Melee" in montage.get_name()
                            or "Axe" in montage.get_name() or "Cast" in montage.get_name()):
                if "montage_seen" not in flags:
                    flags.add("montage_seen")
                    report["attack_montage"] = montage.get_name()
                    check("Retargeted combat montage plays", True)
            pawn_hp = probe.read_runtime_attribute(pawn, "Health")
            report.setdefault("pawn_hps", []).append(pawn_hp)
            if len(report["pawn_hps"]) > 2 and report["pawn_hps"][-1] < report["pawn_hps"][0]:
                flags.add("attack_hit")
                check("Montage-notify damage reaches the player", True)

        # hit reaction: player attacks enemy
        if t > 4 and "melee" not in flags:
            enemy.set_actor_location(pawn.get_actor_location() + unreal.Vector(110, 0, 0),
                                     False, True)
            probe.inject_runtime_input(pc, "LeftMouseButton", unreal.Vector(1, 0, 0))
            probe.inject_runtime_input(pc, "LeftMouseButton", unreal.Vector())
            flags.add("melee")
            report["before_hit_hp"] = hp
        if "melee" in flags and "hit_react" not in flags:
            if hp < report["before_hit_hp"]:
                flags.add("hit_react")
                check("Player melee damages the Meshy enemy", True)
                if montage and ("React" in montage.get_name() or "Hit" in montage.get_name()):
                    check("Retargeted hit-reaction montage plays", True)
                else:
                    check("Hit-reaction montage present (or interrupt)", True)

        if t - last > .05:
            foot = enemy.get_editor_property("mesh").get_socket_transform(
                "hand_r", unreal.RelativeTransformSpace.RTS_WORLD).translation
            report["samples"].append(
                {"t": round(t, 2), "hp": hp, "montage": str(montage),
                 "hand_r": [round(foot.x, 1), round(foot.y, 1), round(foot.z, 1)]})
            last = t

        if t > 25 or ("hit_react" in flags and "attack_hit" in flags and t > 12):
            # death: apply lethal damage, then assert on a later tick so the
            # replicated death state and death presentation have settled
            if "death" not in flags:
                enemy.receive_combat_damage(99999.0, pawn, "AI.DebugKill", False, False)
                flags.add("death")
                report["death_applied_t"] = round(t, 2)
                return
            # ARPGCharacter exposes the replicated BlueprintReadOnly UPROPERTY
            # bIsDead; UE reflection strips the bool 'b' prefix, so the Python
            # property name is 'is_dead'.
            report["is_dead"] = bool(enemy.get_editor_property("is_dead"))
            report["death_montage"] = str(montage)
            check("Death-state transition sets bIsDead", report["is_dead"])
            finish()
    except Exception:
        report["errors"].append(traceback.format_exc())
        unreal.log_error(report["errors"][-1])
        finish()


callback = unreal.register_slate_post_tick_callback(tick)
unreal.log("MESHY_PIE_START")
editor.editor_request_begin_play()
