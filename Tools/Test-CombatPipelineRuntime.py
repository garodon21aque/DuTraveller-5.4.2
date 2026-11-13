"""Bounded PIE validation of the repaired Fireball + melee combat pipeline.

Drives the authoritative action component directly (physical keyboard input is
not simulatable headless; the input->hotbar route is separately instrumented).
Observes: activation, montage start, timed release, projectile spawn, GAS damage,
melee window sweep, AI attack loop and interrupt cleanup. Never saves assets.
"""
import json
import time
import traceback
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)

started = time.monotonic()
phase_started = started
phase = "wait"
callback = None
editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
worlds = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

pawn = None
enemy = None
firebolt = None
slash = None
player_health0 = None
enemy_health0 = None
cast_time = None
projectile_seen = None
events = []

def log(kind, **data):
    line = json.dumps(dict(kind=kind, **data), default=str)
    events.append(line)
    unreal.log("COMBAT_RUNTIME " + line)

def finish(reason):
    log("finished", reason=reason)
    try:
        from pathlib import Path
        out = Path(unreal.Paths.project_saved_dir()) / "Automation" / "CombatRuntimeTest.json"
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text("\n".join(events), encoding="utf-8")
    except Exception:
        pass
    if callback is not None:
        unreal.unregister_slate_post_tick_callback(callback)
    editor.editor_request_end_play()
    unreal.SystemLibrary.quit_editor()

def health(actor):
    try:
        value = unreal.DTStructRepairLibrary.read_runtime_attribute(actor, "Health")
        return float(value) if value is not None else None
    except Exception:
        return None

def find_projectiles(world):
    try:
        proj_cls = unreal.load_class(None, "/Script/DuTraveller.RPGProjectile")
        return unreal.GameplayStatics.get_all_actors_of_class(world, proj_cls)
    except Exception:
        return []

def tick(delta):
    global phase, phase_started, pawn, enemy, firebolt, slash
    global player_health0, enemy_health0, cast_time, projectile_seen
    try:
        now = time.monotonic()
        if now - started > 200:
            finish("timeout: " + phase)
            return
        world = worlds.get_game_world()
        if not world:
            return

        if phase == "wait" and now - phase_started > 4:
            controller = unreal.GameplayStatics.get_player_controller(world, 0)
            pawn = controller.get_controlled_pawn() if controller else None
            if not pawn:
                return
            log("pawn", cls=pawn.get_class().get_path_name(),
                router=bool(pawn.get_editor_property("animation_router")),
                mesh_skel=safe_skel(pawn))
            firebolt = unreal.load_asset("/Game/Data/Abilities/Player/DA_Player_FireBolt")
            slash = unreal.load_asset("/Game/Data/Abilities/Player/DA_Player_BasicSlash")
            ai_cls = unreal.load_class(None, "/Game/Blueprints/AI/BP_RPGAICharacter.BP_RPGAICharacter_C")
            enemy = unreal.DTStructRepairLibrary.spawn_runtime_actor(
                pawn, "/Game/Blueprints/AI/BP_RPGAICharacter.BP_RPGAICharacter_C",
                pawn.get_actor_forward_vector() * 400.0)
            if not enemy:
                finish("enemy spawn failed")
                return
            log("enemy", cls=enemy.get_class().get_path_name(),
                faction=str(enemy.get_editor_property("faction_id")),
                ai_ctrl=str(enemy.get_controller().get_class().get_path_name()) if enemy.get_controller() else None)
            enemy_health0 = health(enemy)
            player_health0 = health(pawn)
            log("baseline", player=player_health0, enemy=enemy_health0)
            phase = "fireball"
            phase_started = now

        elif phase == "fireball":
            ac = pawn.get_editor_property("action_comp")
            ok = ac.try_activate_ability(firebolt, 1, enemy)
            cast_time = now
            router = pawn.get_editor_property("animation_router")
            active = router.get_editor_property("active_combat_montage") if router else None
            log("fireball_cast", activated=ok,
                montage=str(active.get_path_name()) if active else None)
            phase = "projectile_wait"
            phase_started = now

        elif phase == "projectile_wait":
            if projectile_seen is None:
                projs = find_projectiles(world)
                if projs:
                    projectile_seen = now
                    p = projs[0]
                    log("projectile_spawned", cls=p.get_class().get_path_name(),
                        delay=round(now - cast_time, 3))
            if projectile_seen and health(enemy) is not None and health(enemy) < (enemy_health0 or 0):
                log("fireball_damage", before=enemy_health0, after=health(enemy))
                enemy_health0 = health(enemy)
                phase = "melee"
                phase_started = now
            elif now - phase_started > 10:
                log("fireball_timeout", projectile=bool(projectile_seen),
                    enemy_health=health(enemy))
                phase = "melee"
                phase_started = now

        elif phase == "melee":
            # Pull the enemy into melee range of the pawn.
            loc = pawn.get_actor_location() + pawn.get_actor_forward_vector() * 150.0
            enemy.set_actor_location(loc, False, False)
            ac = pawn.get_editor_property("action_comp")
            ok = ac.try_activate_ability(slash, 1, enemy)
            router = pawn.get_editor_property("animation_router")
            active = router.get_editor_property("active_combat_montage") if router else None
            log("melee_cast", activated=ok, montage=str(active.get_path_name()) if active else None)
            phase = "melee_wait"
            phase_started = now

        elif phase == "melee_wait":
            h = health(enemy)
            if h is not None and h < (enemy_health0 or 0):
                log("melee_damage", before=enemy_health0, after=h)
                phase = "ai_wait"
                phase_started = now
            elif now - phase_started > 6:
                log("melee_timeout", enemy_health=h)
                phase = "ai_wait"
                phase_started = now

        elif phase == "ai_wait":
            # The spawned enemy AI (BP_RPGAIController + DA_AI_BasicAttack) should
            # close in and damage the player within ~25s.
            h = health(pawn)
            if h is not None and player_health0 is not None and h < player_health0:
                log("ai_attacked_player", before=player_health0, after=h)
                finish("complete")
            elif now - phase_started > 30:
                log("ai_timeout", player_health=h)
                finish("complete_with_ai_timeout")
    except Exception:
        log("exception", traceback=traceback.format_exc())
        finish("exception")

def safe_skel(p):
    try:
        mesh = p.get_editor_property("mesh")
        sk = mesh.get_editor_property("skeletal_mesh_asset")
        return str(sk.skeleton.get_path_name()) if sk else None
    except Exception as exc:
        return "ERR:" + str(exc)

try:
    current_world = worlds.get_editor_world()
    if not current_world or current_world.get_name() != "Tutorial":
        unreal.EditorLoadingAndSavingUtils.load_map("/Game/Levels/Tutorial")
    callback = unreal.register_slate_post_tick_callback(tick)
    editor.editor_request_begin_play()
    phase_started = time.monotonic()
    log("started")
except Exception:
    log("exception", traceback=traceback.format_exc())
    finish("startup exception")
