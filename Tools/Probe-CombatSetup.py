"""Read-only probe: AI characters, PlayerState skill config, AI controller defaults."""
import json
from pathlib import Path
import unreal

report = {"ai_characters": {}, "player_state": {}, "ai_controller": {},
          "extra": {}, "errors": []}

def safe(fn, *a, **k):
    try:
        return fn(*a, **k)
    except Exception as exc:
        return "ERR:" + str(exc)

# AI characters
for path in ["/Game/Blueprints/AI/BP_RPGAICharacter",
             "/Game/Blueprints/AI/BP_AIRangedCharacter",
             "/Game/Blueprints/Player/BP_PlayerCharacter",
             "/Game/Blueprints/Player/BP_RPGCharacter"]:
    try:
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            report["ai_characters"][path] = {"missing": True}
            continue
        bp = unreal.load_asset(path)
        cdo = unreal.get_default_object(bp.generated_class())
        entry = {"parent": str(safe(lambda: unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp)))}
        mesh = cdo.get_editor_property("mesh")
        if mesh:
            sk = mesh.get_editor_property("skeletal_mesh_asset") or mesh.get_editor_property("skeletal_mesh")
            entry["skeletal_mesh"] = sk.get_path_name() if sk else None
            if sk:
                entry["skeleton"] = safe(lambda: sk.skeleton.get_path_name())
            for prop in ("anim_class", "anim_instance_class"):
                v = safe(mesh.get_editor_property, prop)
                if not str(v).startswith("ERR"):
                    entry["anim_class"] = str(v.get_path_name() if v else v)
        entry["faction_id"] = str(safe(cdo.get_editor_property, "faction_id"))
        entry["ai_controller_class"] = str(safe(cdo.get_editor_property, "ai_controller_class"))
        router = cdo.get_editor_property("animation_router")
        if router:
            anims = {}
            try:
                for k, v in router.get_editor_property("combat_animations").items():
                    m = v.get_editor_property("montage")
                    anims[str(k)] = m.get_path_name() if m else None
            except Exception as exc:
                anims["err"] = str(exc)
            entry["combat_animations"] = anims
        ac = cdo.get_editor_property("action_comp")
        if ac:
            entry["action_comp_class"] = ac.get_class().get_path_name()
            for prop in ("melee_trace_sockets", "melee_trace_radius"):
                v = safe(ac.get_editor_property, prop)
                if not str(v).startswith("ERR"):
                    entry[prop] = str(v)
        report["ai_characters"][path] = entry
    except Exception as exc:
        report["errors"].append(path + ": " + str(exc))

# PlayerState / skills
for path in ["/Game/Blueprints/Framework/BP_PlayerState",
             "/Game/Blueprints/Player/BP_RPGPlayerController"]:
    try:
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            report["player_state"][path] = {"missing": True}
            continue
        bp = unreal.load_asset(path)
        cdo = unreal.get_default_object(bp.generated_class())
        entry = {"parent": str(safe(lambda: unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp)))}
        skills = safe(cdo.get_editor_property, "skills") or safe(cdo.get_editor_property, "skill_comp")
        if skills and not str(skills).startswith("ERR"):
            try:
                sh = skills.get_editor_property("starter_hotbar")
                entry["starter_hotbar"] = [
                    {"slot": int(s.get_editor_property("slot")),
                     "def": s.get_editor_property("definition").get_path_name() if s.get_editor_property("definition") else None}
                    for s in sh]
            except Exception as exc:
                entry["starter_hotbar"] = "ERR:" + str(exc)
            aa = skills.get_editor_property("available_abilities")
            entry["available_abilities"] = [d.get_path_name() if d else None for d in aa]
            cat = skills.get_editor_property("ability_catalogue")
            entry["ability_catalogue"] = cat.get_path_name() if cat else None
        else:
            entry["skills"] = str(skills)
        report["player_state"][path] = entry
    except Exception as exc:
        report["errors"].append(path + ": " + str(exc))

# RPGAIController CDO defaults
try:
    cdo = unreal.get_default_object(unreal.load_class(None, "/Script/DuTraveller.RPGAIController"))
    report["ai_controller"] = {
        "attack_ability": safe(lambda: cdo.get_editor_property("attack_ability").get_path_name()
                               if cdo.get_editor_property("attack_ability") else None),
        "detection_range": safe(cdo.get_editor_property, "detection_range"),
    }
except Exception as exc:
    report["errors"].append("ai controller: " + str(exc))

# Action component / notify / projectile-ish BPs
for path in ["/Game/Blueprints/AI/NPC/BP_AIRPGActionComponent",
             "/Game/Blueprints/Notify/BP_DealDamageNotify",
             "/Game/Blueprints/Actions/Melee/BP_RPGAction_MeleeAttack",
             "/Game/Data/Abilities/Actions/BP_DT_BasicSlashAction"]:
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        bp = unreal.load_asset(path)
        report["extra"][path] = str(safe(lambda: unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp)))
    else:
        report["extra"][path] = "missing"

# Retarget assets present?
for p in ["/Game/DTIntegration/Animation/IK_DT_LegacyUE4",
          "/Game/DTIntegration/Animation/IK_DT_UEFN",
          "/Game/DTIntegration/Animation/RTG_DT_LegacyUE4_UEFN"]:
    report["extra"][p] = "exists" if unreal.EditorAssetLibrary.does_asset_exist(p) else "missing"

# GameplayTags are validated natively by the notify-authoring helpers.

out = Path(unreal.Paths.project_saved_dir()) / "Automation" / "CombatSetupProbe.json"
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(report, indent=2, default=str), encoding="utf-8")
unreal.log("PROBE_DONE")
unreal.SystemLibrary.quit_editor()
