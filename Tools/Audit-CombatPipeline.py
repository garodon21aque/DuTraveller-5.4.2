"""Audit the live combat/animation pipeline for the active runtime character.

Reports machine-readable JSON to Saved/Automation/CombatPipelineAudit.json.
Never saves or modifies any asset.
"""
import json
from pathlib import Path
import unreal

report = {"ability_definitions": [], "live_character": {}, "montages": [],
          "anim_bp": {}, "notifies": [], "actions": [], "errors": []}

def safe(fn, *a, **k):
    try:
        return fn(*a, **k)
    except Exception as exc:
        return "ERR:" + str(exc)

# ---------------------------------------------------------------- abilities
for path in ["/Game/Data/Abilities/Player/DA_Player_BasicSlash",
             "/Game/Data/Abilities/Player/DA_Player_FireBolt",
             "/Game/Data/Abilities/Player/DA_Player_FullSwing",
             "/Game/Data/Abilities/Player/DA_Player_IceShards",
             "/Game/Data/Abilities/DA_AI_BasicAttack"]:
    try:
        da = unreal.load_asset(path)
        if not da:
            report["ability_definitions"].append({"asset": path, "missing": True})
            continue
        entry = {"asset": path}
        for prop in ("ability_id", "display_name", "kind", "maximum_rank",
                     "required_player_level", "base_damage_by_rank",
                     "attack_power_scale", "ability_power_scale", "mana_cost",
                     "stamina_cost", "range", "cooldown", "requires_target",
                     "allow_friendly_fire", "action_class", "presentation"):
            v = safe(da.get_editor_property, prop)
            entry[prop] = str(v) if not isinstance(v, (int, float, bool, str, type(None), list)) else v
        report["ability_definitions"].append(entry)
    except Exception as exc:
        report["errors"].append(path + ": " + str(exc))

# ---------------------------------------------------------------- live pawn
def describe_mesh(char_cdo):
    out = {}
    mesh = char_cdo.get_editor_property("mesh")
    if not mesh:
        return {"mesh": None}
    skel_mesh = mesh.get_editor_property("skeletal_mesh_asset") or mesh.get_editor_property("skeletal_mesh")
    out["skeletal_mesh"] = skel_mesh.get_path_name() if skel_mesh else None
    if skel_mesh:
        out["skeleton"] = safe(lambda: skel_mesh.skeleton.get_path_name())
        sockets = [s.get_name() for s in skel_mesh.find_skeletal_mesh_sockets(skel_mesh)] if hasattr(skel_mesh, 'find_skeletal_mesh_sockets') else []
        out["socket_probe"] = {name: mesh.does_socket_exist(name) for name in
                               ("hand_r", "hand_l", "weapon_r", "weapon", "spine_03",
                                "RightHand", "LeftHand", "weapon_socket", "spell_socket")}
    anim_class = mesh.get_editor_property("anim_class") or mesh.get_editor_property("anim_instance_class")
    out["anim_class"] = anim_class.get_path_name() if anim_class else None
    out["anim_bp_generated_parent"] = None
    if anim_class:
        try:
            pkg = unreal.find_asset(anim_class.get_path_name().split(".")[0])
            out["anim_bp_generated_parent"] = str(unreal.BlueprintEditorLibrary.get_blueprint_parent_class(pkg))
        except Exception as exc:
            out["anim_bp_parent_err"] = str(exc)
    return out

for path in ["/Game/Blueprints/Characters/BP_PlayerCharacter",
             "/Game/Blueprints/Player/BP_PlayerCharacter",
             "/Game/Blueprints/AI/BP_RPGAICharacter",
             "/Game/Blueprints/AI/BP_AIRangedCharacter"]:
    try:
        bp = unreal.load_asset(path)
        if not bp:
            report["live_character"][path] = {"missing": True}
            continue
        cdo = unreal.get_default_object(bp.generated_class())
        entry = {"parent": str(unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp))}
        entry["mesh"] = describe_mesh(cdo)
        router = cdo.get_editor_property("animation_router")
        anims = {}
        if router:
            for k, v in router.get_editor_property("combat_animations").items():
                m = v.get_editor_property("montage")
                anims[str(k)] = {"montage": m.get_path_name() if m else None,
                                 "section": str(v.get_editor_property("start_section")),
                                 "montage_skeleton": safe(lambda mm=m: mm.get_skeleton().get_path_name()) if m else None}
            hr = router.get_editor_property("hit_reaction").get_editor_property("montage")
            entry["hit_reaction_montage"] = hr.get_path_name() if hr else None
        entry["combat_animations"] = anims
        entry["starting_equipment"] = [c.get_path_name() for c in cdo.get_editor_property("starting_equipment") if c]
        entry["faction_id"] = str(safe(cdo.get_editor_property, "faction_id"))
        ai = safe(cdo.get_editor_property, "ai_controller_class")
        entry["ai_controller_class"] = str(ai)
        report["live_character"][path] = entry
    except Exception as exc:
        report["errors"].append(path + ": " + str(exc))

# ---------------------------------------------------------------- montages
try:
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    found = []
    for ad in reg.get_assets_by_path("/Game", True):
        if ad.asset_class_path.asset_name == "AnimMontage":
            p = str(ad.package_name)
            if any(k in p.lower() for k in ("cast", "spell", "magic", "slash", "melee", "attack", "hit", "death", "stun", "buff", "summon")):
                found.append(p)
    for p in sorted(found):
        m = unreal.load_asset(p)
        if not m:
            continue
        notifies = []
        try:
            for n in m.get_editor_property("notifies"):
                notify_obj = n.get_editor_property("notify")
                state_obj = n.get_editor_property("notify_state_class")
                cls = notify_obj.get_class().get_name() if notify_obj else (state_obj.get_class().get_name() if state_obj else "?")
                tag = ""
                if notify_obj:
                    tag = str(safe(notify_obj.get_editor_property, "event_tag"))
                notifies.append({"class": cls, "tag": tag,
                                 "trigger_time": round(float(safe(n.get_editor_property, "trigger_time") or 0), 3)})
        except Exception:
            pass
        slots = []
        try:
            slots = [str(m.get_editor_property("slot_animation_tracks"))]
        except Exception:
            pass
        report["montages"].append({
            "asset": p,
            "skeleton": safe(lambda: m.get_skeleton().get_path_name()),
            "length": round(float(safe(m.get_editor_property, "sequence_length") or 0), 3),
            "notifies": notifies,
            "slot_tracks": slots,
        })
except Exception as exc:
    report["errors"].append("montage scan: " + str(exc))

# ---------------------------------------------------------------- AnimBP graph (montage slots / linked layers)
try:
    for p in ["/Game/Art/3D/Player/Animations/Anims/AnimBP_RPGCharacter",
              "/Game/Blueprints/Animations/ABP_SandboxCharacter"]:
        if not unreal.EditorAssetLibrary.does_asset_exist(p):
            continue
        bp = unreal.load_asset(p)
        entry = {"parent": str(safe(lambda: unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp)))}
        # find AnimGraph nodes by name fragments
        names = []
        try:
            for obj in unreal.load_assets_from_path(p.rsplit("/", 1)[0]) if False else []:
                pass
        except Exception:
            pass
        report["anim_bp"][p] = entry
except Exception as exc:
    report["errors"].append("anim_bp: " + str(exc))

# ---------------------------------------------------------------- notify + action BPs
for path in ["/Game/Blueprints/Notify/BP_DealDamageNotify",
             "/Game/Blueprints/Actions/Melee/BP_Action_Swing",
             "/Game/Blueprints/Actions/Melee/BP_RPGAction_MeleeAttack",
             "/Game/Data/Abilities/Actions/BP_DT_BasicSlashAction",
             "/Game/Blueprints/AI/NPC/BP_AIRPGActionComponent"]:
    try:
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            report["actions"].append({"asset": path, "missing": True})
            continue
        bp = unreal.load_asset(path)
        report["actions"].append({"asset": path,
                                  "parent": str(safe(lambda: unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp))),
                                  "status": str(bp.get_editor_property("status"))})
    except Exception as exc:
        report["errors"].append(path + ": " + str(exc))

# projectiles / effect actors
try:
    hits = []
    for ad in reg.get_assets_by_path("/Game", True):
        p = str(ad.package_name).lower()
        if any(k in p for k in ("projectile", "fireball", "firebolt", "spelleffect", "spell_effect")):
            hits.append(str(ad.package_name))
    report["projectile_like_assets"] = hits
except Exception as exc:
    report["errors"].append("projectile scan: " + str(exc))

out = Path(unreal.Paths.project_saved_dir()) / "Automation" / "CombatPipelineAudit.json"
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(report, indent=2, default=str), encoding="utf-8")
unreal.log("AUDIT_DONE")
unreal.SystemLibrary.quit_editor()
