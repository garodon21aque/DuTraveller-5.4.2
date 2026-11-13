"""Live PIE gameplay audit for the unified DT/RPG player hierarchy.

Invocation (must use -ExecutePythonScript so the engine keeps ticking;
-run=pythonscript is a commandlet that exits before PIE):
  UnrealEditor-Cmd.exe DT.uproject /Game/Levels/Tutorial -unattended -nullrhi
    -nosound -NoAutoSave -ExecutePythonScript="<this file>" -abslog=<log>

Phase A (editor world, no PIE): Tutorial map GameMode override, framework
defaults, pawn CDO mesh/anim/movement config, AnimBP skeleton + anim-graph
node inventory, montage slot tracks vs AnimBP slots, ability definitions,
starter hotbar and input mapping context.

Phase B (PIE): actual spawned pawn, runtime AnimInstance class, movement
state, sampled pose deltas while driving movement and montages, hotbar slot
activation through the real controller path, projectile spawn, damage, mana
spend and regeneration, HUD presence.

Reports PIEAUDIT json lines; writes Saved/Automation/PIEGameplayAudit.json.
Never saves assets.
"""
import json
import time
import traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)

repair = unreal.DTStructRepairLibrary
events = []


def log(kind, **data):
    line = json.dumps(dict(kind=kind, **data), default=str)
    events.append(line)
    unreal.log("PIEAUDIT " + line)


def safe(fn, *a, **k):
    try:
        return fn(*a, **k)
    except Exception as exc:
        return "ERR:" + str(exc)[:160]


def path_of(obj):
    try:
        return obj.get_path_name() if obj else None
    except Exception:
        return str(obj)


RPG_ATTRS_CLS = "/Script/DuTraveller.RPGAttributeSet"


def attr(asc, name):
    """Float attribute value via the ASC blueprint library (authoritative)."""
    try:
        cls = unreal.load_class(None, RPG_ATTRS_CLS)
        ok, value = unreal.AbilitySystemBlueprintLibrary.get_float_attribute_from_ability_system_component(
            asc, cls, name)
        if ok:
            return float(value)
    except Exception:
        pass
    try:
        cls = unreal.load_class(None, RPG_ATTRS_CLS)
        ok, value = unreal.AbilitySystemBlueprintLibrary.get_float_attribute_base_from_ability_system_component(
            asc, cls, name)
        if ok:
            return float(value)
    except Exception:
        pass
    return None


def resource(actor, name):
    """AttributeComp mirror (what legacy HUD widgets read)."""
    try:
        comp = actor.get_editor_property("attribute_comp")
        return float(comp.get_editor_property(name.lower()))
    except Exception:
        return None


# ---------------------------------------------------------------- Phase A --
def static_phase():
    rep = {}
    worlds = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    current = worlds.get_editor_world()
    if not current or current.get_name() != "Tutorial":
        unreal.EditorLoadingAndSavingUtils.load_map("/Game/Levels/Tutorial")
    world = worlds.get_editor_world()
    ws = world.get_world_settings()
    rep["map_gamemode_override"] = str(safe(lambda: ws.get_editor_property("default_game_mode").get_path_name()))

    gm_bp = unreal.load_asset("/Game/Blueprints/Framework/BP_RPGGameModeBase")
    if gm_bp:
        gm = unreal.get_default_object(gm_bp.generated_class())
        rep["gamemode"] = {
            "status": str(safe(gm_bp.get_editor_property, "status")),
            "pawn": str(safe(lambda: gm.get_editor_property("default_pawn_class").get_path_name())),
            "controller": str(safe(lambda: gm.get_editor_property("player_controller_class").get_path_name())),
            "player_state": str(safe(lambda: gm.get_editor_property("player_state_class").get_path_name())),
            "game_state": str(safe(lambda: gm.get_editor_property("game_state_class").get_path_name())),
        }

    pawn_bp = unreal.load_asset("/Game/Blueprints/Characters/BP_PlayerCharacter")
    if pawn_bp:
        cdo = unreal.get_default_object(pawn_bp.generated_class())
        entry = {
            "status": str(safe(pawn_bp.get_editor_property, "status")),
            "bp_parent": str(safe(lambda: unreal.BlueprintEditorLibrary.get_blueprint_parent_class(pawn_bp).get_path_name())),
            "genclass_super": str(safe(lambda: pawn_bp.generated_class().get_super_class().get_path_name())),
            "faction": str(safe(cdo.get_editor_property, "faction_id")),
            "auto_possess_player": str(safe(cdo.get_editor_property, "auto_possess_player")),
            "use_controller_yaw": str(safe(lambda: cdo.get_editor_property("use_controller_rotation_yaw"))),
            "starting_equipment": str(safe(lambda: [str(x) for x in cdo.get_editor_property("starting_equipment")])),
        }
        mesh = safe(cdo.get_editor_property, "mesh")
        if mesh and not (isinstance(mesh, str) and mesh.startswith("ERR")):
            sk = safe(mesh.get_editor_property, "skeletal_mesh_asset")
            entry["mesh"] = {
                "asset": path_of(sk),
                "skeleton": str(safe(lambda: sk.skeleton.get_path_name())),
                "anim_class": str(safe(lambda: mesh.get_editor_property("anim_class").get_path_name())),
                "anim_mode": str(safe(mesh.get_editor_property, "animation_mode")),
                "tick_option": str(safe(mesh.get_editor_property, "visibility_based_anim_tick_option")),
            }
        mv = safe(cdo.get_editor_property, "character_movement")
        if mv and not (isinstance(mv, str) and mv.startswith("ERR")):
            entry["movement"] = {
                "max_walk": safe(mv.get_editor_property, "max_walk_speed"),
                "max_walk_crouched": safe(mv.get_editor_property, "max_walk_speed_crouched"),
                "jump_z": safe(mv.get_editor_property, "jump_z_velocity"),
                "gravity": safe(mv.get_editor_property, "gravity_scale"),
                "orient_to_movement": safe(mv.get_editor_property, "b_orient_rotation_to_movement"),
                "rotation_rate": str(safe(mv.get_editor_property, "rotation_rate")),
                "air_control": safe(mv.get_editor_property, "air_control"),
            }
        router = safe(cdo.get_editor_property, "animation_router")
        if router and not (isinstance(router, str) and router.startswith("ERR")):
            anims = {}
            combat_map = safe(router.get_editor_property, "combat_animations")
            if not isinstance(combat_map, str):
                for k, v in combat_map.items():
                    anims[str(k)] = {
                        "montage": path_of(safe(v.get_editor_property, "montage")),
                        "start_section": str(safe(v.get_editor_property, "start_section")),
                        "play_rate": safe(v.get_editor_property, "play_rate"),
                    }
            hr = safe(router.get_editor_property, "hit_reaction")
            entry["router"] = {
                "combat_animations": anims,
                "hit_reaction": str(safe(lambda: hr.get_editor_property("montage").get_path_name())),
            }
        ac = safe(cdo.get_editor_property, "action_comp")
        if ac and not (isinstance(ac, str) and ac.startswith("ERR")):
            entry["action_comp"] = {
                "melee_sockets": str(safe(ac.get_editor_property, "melee_trace_sockets")),
                "fallback_release": safe(ac.get_editor_property, "fallback_release_fraction"),
            }
        rep["player_pawn"] = entry

    for abp_path in ["/Game/Blueprints/Animations/ABP_SandboxCharacter",
                     "/Game/Art/3D/Player/Animations/Anims/AnimBP_RPGCharacter",
                     "/Game/Art/3D/Player/Animations/Anims/AnimBP_RPGAICharacter"]:
        if unreal.EditorAssetLibrary.does_asset_exist(abp_path):
            rep.setdefault("anim_graphs", {})[abp_path] = [str(x) for x in repair.describe_anim_graph(abp_path)]

    for mp in ["/Game/DTIntegration/Animation/Combat/DT_Standing_1H_Cast_Spell_01_UEFN",
               "/Game/DTIntegration/Animation/Combat/DT_Standing_Melee_Attack_Horizontal_UEFN",
               "/Game/DTIntegration/Animation/Combat/DT_Standing_Melee_Attack_360_High_UEFN",
               "/Game/DTIntegration/Animation/Combat/DT_AMAI_ReactHitFont_UEFN",
               "/Game/Art/3D/Player/Animations/Anims/Retargeted/Sword_And_Shield_Slash_mixamo_com_Montage"]:
        if unreal.EditorAssetLibrary.does_asset_exist(mp):
            rep.setdefault("montages", {})[mp] = [str(x) for x in repair.describe_montage_notifies(mp)]

    for ap in ["/Game/Data/Abilities/Player/DA_Player_FireBolt",
               "/Game/Data/Abilities/Player/DA_Player_IceShards",
               "/Game/Data/Abilities/Player/DA_Player_BasicSlash",
               "/Game/Data/Abilities/Player/DA_Player_FullSwing",
               "/Game/Data/Abilities/DA_AI_BasicAttack"]:
        da = unreal.load_asset(ap)
        if not da:
            continue
        tag = safe(da.get_editor_property, "release_event_tag")
        rep.setdefault("abilities", {})[ap] = {
            "ability_id": str(safe(da.get_editor_property, "ability_id")),
            "kind": str(safe(da.get_editor_property, "kind")),
            "mana_cost": safe(da.get_editor_property, "mana_cost"),
            "stamina_cost": safe(da.get_editor_property, "stamina_cost"),
            "range": safe(da.get_editor_property, "range"),
            "cooldown": safe(da.get_editor_property, "cooldown"),
            "requires_target": safe(da.get_editor_property, "b_requires_target"),
            "release_tag": str(getattr(tag, "tag_name", tag)),
            "projectile": str(safe(lambda: da.get_editor_property("projectile_class").get_path_name())),
            "release_socket": str(safe(da.get_editor_property, "release_socket")),
            "max_rank": safe(da.get_editor_property, "maximum_rank"),
            "base_damage": str(safe(da.get_editor_property, "base_damage_by_rank")),
        }

    ps_bp = unreal.load_asset("/Game/Blueprints/Framework/BP_PlayerState")
    if ps_bp:
        ps = unreal.get_default_object(ps_bp.generated_class())
        skills = safe(ps.get_editor_property, "skills")
        hotbar = []
        if skills and not isinstance(skills, str):
            for s in safe(skills.get_editor_property, "starter_hotbar"):
                d = s.get_editor_property("definition")
                hotbar.append({"slot": int(s.get_editor_property("slot")), "def": path_of(d)})
            rep["player_state"] = {
                "status": str(safe(ps_bp.get_editor_property, "status")),
                "starter_hotbar": hotbar,
                "available_abilities": str(safe(lambda: [path_of(d) for d in skills.get_editor_property("available_abilities")])),
                "catalogue": str(safe(lambda: skills.get_editor_property("ability_catalogue").get_path_name())),
                "start_attr_points": safe(ps.get_editor_property, "starting_attribute_points"),
                "start_skill_points": safe(ps.get_editor_property, "starting_skill_points"),
            }

    pc_bp = unreal.load_asset("/Game/Blueprints/Framework/BP_PlayerController")
    if pc_bp:
        pc = unreal.get_default_object(pc_bp.generated_class())
        rep["player_controller"] = {
            "status": str(safe(pc_bp.get_editor_property, "status")),
            "parent": str(safe(lambda: unreal.BlueprintEditorLibrary.get_blueprint_parent_class(pc_bp).get_path_name())),
            "hotbar_input": safe(pc.get_editor_property, "b_enable_combat_hotbar_input"),
            "hud_class": str(safe(lambda: pc.get_editor_property("hud_widget_class").get_path_name())),
        }

    imc = unreal.load_asset("/Game/Input/IMC_GDCMotionMatching")
    if imc:
        maps = []
        for m in safe(imc.get_editor_property, "mappings"):
            maps.append({"action": path_of(safe(lambda mm=m: mm.get_editor_property("action"))),
                         "key": str(safe(lambda mm=m: mm.get_editor_property("key")))})
        rep["input_context"] = {"mappings": maps}

    log("static", data=rep)


# ---------------------------------------------------------------- Phase B --
started = time.monotonic()
phase_started = started
phase = "pie_wait"
callback = None
editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
worlds = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

pawn = None
enemy = None
samples = []
cast_time = None
projectile_seen = None
enemy_health0 = None
melee_health0 = None
move_start = None
tick_num = 0

ANIM_VAR_CANDIDATES = [
    "ai_character", "move_comp", "b_can_launch_spell", "b_is_stunned", "b_in_shock_loop",
    "speed", "velocity", "ground_speed", "direction", "is_falling", "is_in_air",
    "is_moving", "b_is_moving", "has_movement_input", "is_accelerating",
    "movement_state", "locomotion_state", "gait", "stance", "rotation_mode",
    "current_gait", "is_jumping", "is_crouched", "locomotion_direction",
]


def anim_vars(anim):
    out = {}
    for name in ANIM_VAR_CANDIDATES:
        try:
            out[name] = anim.get_editor_property(name)
        except Exception:
            continue
    return {k: str(v)[:80] for k, v in out.items()}


def bone_delta(bone):
    try:
        t = pawn.get_editor_property("mesh").get_socket_transform(bone)
        p = pawn.get_actor_location()
        loc = t.translation
        return [round(loc.x - p.x, 1), round(loc.y - p.y, 1), round(loc.z - p.z, 1)]
    except Exception:
        return None


def mana_now(asc=None):
    if asc is not None:
        v = attr(asc, "Mana")
        if v is not None:
            return v
    return resource(pawn, "Mana")


def health_now(actor, asc=None):
    v = attr(asc, "Health") if asc is not None else None
    if v is None:
        v = resource(actor, "Health")
    return v


def snapshot(tag):
    try:
        mesh = pawn.get_editor_property("mesh")
        anim = mesh.get_anim_instance() if mesh else None
        mv = pawn.get_editor_property("character_movement")
        vel = pawn.get_velocity()
        router = pawn.get_editor_property("animation_router")
        active = router.get_editor_property("active_combat_montage") if router else None
        montage_pos = None
        any_montage = None
        if anim and active:
            any_montage = anim.is_any_montage_playing()
            montage_pos = safe(lambda: anim.montage_get_position(active))
        samples.append({
            "t": round(time.monotonic() - started, 2), "tag": tag,
            "pos": str(pawn.get_actor_location()),
            "vel2d": round((vel.x ** 2 + vel.y ** 2) ** 0.5, 1),
            "velz": round(vel.z, 1),
            "move_mode": str(safe(mv.get_editor_property, "movement_mode")),
            "is_falling": str(safe(mv.is_falling)),
            "montage": path_of(active), "montage_pos": montage_pos,
            "any_montage": any_montage,
            "bone_hand_r": bone_delta("hand_r"),
            "bone_foot_l": bone_delta("foot_l"),
            "anim_vars": anim_vars(anim) if anim else {},
            "mana": mana_now(),
        })
    except Exception:
        log("snapshot_error", trace=traceback.format_exc())


def finish(reason):
    log("finished", reason=reason)
    log("samples", data=samples)
    try:
        out = Path(unreal.Paths.project_saved_dir()) / "Automation" / "PIEGameplayAudit.json"
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text("\n".join(events), encoding="utf-8")
    except Exception:
        pass
    if callback is not None:
        unreal.unregister_slate_post_tick_callback(callback)
    try:
        editor.editor_request_end_play()
    except Exception:
        pass
    unreal.SystemLibrary.quit_editor()


def tick(delta):
    global phase, phase_started, pawn, enemy, cast_time, projectile_seen
    global enemy_health0, melee_health0, move_start, tick_num
    try:
        now = time.monotonic()
        tick_num += 1
        if now - started > 300:
            finish("timeout: " + phase)
            return
        world = worlds.get_game_world()
        if not world:
            return

        if phase == "pie_wait" and now - phase_started > 5:
            controller = unreal.GameplayStatics.get_player_controller(world, 0)
            pawn = controller.get_controlled_pawn() if controller else None
            if not pawn:
                if now - phase_started > 30:
                    finish("no pawn spawned")
                return
            gm = unreal.GameplayStatics.get_game_mode(world)
            mesh = pawn.get_editor_property("mesh")
            anim = mesh.get_anim_instance() if mesh else None
            if "-nullrhi" in unreal.SystemLibrary.get_command_line().lower():
                mesh.set_editor_property("visibility_based_anim_tick_option",
                    unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
            ps = safe(controller.get_editor_property, "player_state")
            asc = None
            avatar = None
            try:
                asc = ps.get_editor_property("ability_system") if ps and not isinstance(ps, str) else None
            except Exception:
                pass
            if asc is None:
                asc = safe(pawn.get_editor_property, "fallback_ability_system")
            try:
                avatar = asc.get_avatar_actor()
            except Exception:
                pass
            hotbar_now = []
            try:
                skills = ps.get_editor_property("skills")
                for s in skills.get_editor_property("hotbar"):
                    d = s.get_editor_property("definition")
                    hotbar_now.append({"slot": int(s.get_editor_property("slot")),
                                       "id": str(safe(d.get_editor_property, "ability_id"))})
            except Exception as exc:
                hotbar_now = ["ERR:" + str(exc)]
            active_fx = safe(lambda: [str(x.get_editor_property("def").get_path_name())
                                      if hasattr(x, "get_editor_property") else str(x)
                                      for x in asc.get_active_effects(unreal.GameplayEffectQuery())]) if asc else None
            log("pawn_live", cls=pawn.get_class().get_path_name(),
                gm=gm.get_class().get_path_name() if gm else None,
                controller=controller.get_class().get_path_name(),
                player_state=ps.get_class().get_path_name() if ps and not isinstance(ps, str) else None,
                components=[c.get_class().get_path_name() for c in pawn.get_components_by_class(unreal.ActorComponent)],
                mesh=path_of(safe(mesh.get_editor_property, "skeletal_mesh_asset")),
                mesh_skel=str(safe(lambda: mesh.get_editor_property("skeletal_mesh_asset").skeleton.get_path_name())),
                anim=str(anim.get_class().get_path_name() if anim else None),
                tick_opt=str(safe(mesh.get_editor_property, "visibility_based_anim_tick_option")),
                asc=str(asc.get_path_name() if asc else None),
                asc_avatar=str(avatar.get_class().get_path_name() if avatar else None),
                hotbar=hotbar_now,
                mana=mana_now(asc), health=health_now(pawn, asc),
                max_mana=attr(asc, "MaxMana"),
                mana_regen=attr(asc, "ManaRegeneration"),
                health_regen=attr(asc, "HealthRegeneration"),
                hud=str(safe(lambda: controller.get_editor_property("hud_widget").get_class().get_path_name())),
                active_effects=active_fx)
            move_start = pawn.get_actor_location()
            snapshot("spawn")
            phase = "loco_move"
            phase_started = now

        elif phase == "loco_move":
            if now - phase_started < 2.2:
                pawn.add_movement_input(unreal.Vector(1, 0, 0), 1.0, False)
                if int((now - phase_started) * 4) != int((now - phase_started - delta) * 4):
                    snapshot("move")
            else:
                snapshot("move_end")
                phase = "loco_jump"
                phase_started = now

        elif phase == "loco_jump":
            if now - phase_started < 0.2:
                safe(pawn.jump)
            elif now - phase_started < 2.0:
                if int((now - phase_started) * 4) != int((now - phase_started - delta) * 4):
                    snapshot("jump")
            else:
                phase = "combat_setup"
                phase_started = now

        elif phase == "combat_setup":
            ai_cls = unreal.load_class(None, "/Game/Blueprints/AI/BP_RPGAICharacter.BP_RPGAICharacter_C")
            loc = pawn.get_actor_location() + pawn.get_actor_forward_vector() * 300.0
            enemy = world.spawn_actor(ai_cls, location=loc)
            enemy_health0 = health_now(enemy)
            controller = unreal.GameplayStatics.get_player_controller(world, 0)
            ok_ctrl = safe(lambda: controller.activate_hotbar_slot(1))
            cast_time = now
            snapshot("cast_start")
            log("fireball_hotbar", via_controller=str(ok_ctrl), mana=mana_now(),
                enemy_health=enemy_health0,
                enemy=str(enemy.get_class().get_path_name() if enemy else None))
            phase = "cast_watch"
            phase_started = now

        elif phase == "cast_watch":
            proj_cls = unreal.load_class(None, "/Script/DuTraveller.RPGProjectile")
            projs = unreal.GameplayStatics.get_all_actors_of_class(world, proj_cls)
            if projs and projectile_seen is None:
                projectile_seen = now
                log("projectile", delay=round(now - cast_time, 3),
                    cls=projs[0].get_class().get_path_name())
            if int((now - phase_started) * 4) != int((now - phase_started - delta) * 4):
                snapshot("cast")
            if now - phase_started > 4:
                h = health_now(enemy)
                log("fireball_result", projectile=bool(projectile_seen),
                    enemy_before=enemy_health0, enemy_after=h, mana_now=mana_now())
                loc = pawn.get_actor_location() + pawn.get_actor_forward_vector() * 140.0
                enemy.set_actor_location(loc, False, False)
                melee_health0 = health_now(enemy)
                controller = unreal.GameplayStatics.get_player_controller(world, 0)
                safe(lambda: controller.activate_hotbar_slot(0))
                snapshot("melee_start")
                phase = "melee_watch"
                phase_started = now

        elif phase == "melee_watch":
            if int((now - phase_started) * 4) != int((now - phase_started - delta) * 4):
                snapshot("melee")
            h = health_now(enemy)
            if h is not None and melee_health0 is not None and h < melee_health0:
                log("melee_damage", before=melee_health0, after=h)
                phase = "mana_watch"
                phase_started = now
            elif now - phase_started > 5:
                log("melee_result", enemy=melee_health0, enemy_after=h)
                phase = "mana_watch"
                phase_started = now

        elif phase == "mana_watch":
            if int(now - phase_started) != int(now - phase_started - delta):
                log("mana_tick", mana=mana_now(),
                    stamina=resource(pawn, "Stamina"), health=health_now(pawn))
            if now - phase_started > 8:
                widgets = unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.UserWidget, False)
                log("hud_widgets", widgets=[w.get_class().get_path_name() for w in widgets])
                finish("complete")
    except Exception:
        log("exception", trace=traceback.format_exc())
        finish("exception")


try:
    static_phase()
    current_world = worlds.get_editor_world()
    if not current_world or current_world.get_name() != "Tutorial":
        unreal.EditorLoadingAndSavingUtils.load_map("/Game/Levels/Tutorial")
    callback = unreal.register_slate_post_tick_callback(tick)
    editor.editor_request_begin_play()
    phase_started = time.monotonic()
    log("started")
except Exception:
    log("exception", trace=traceback.format_exc())
    finish("startup exception")
