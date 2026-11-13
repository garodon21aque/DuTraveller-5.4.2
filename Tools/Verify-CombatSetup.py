"""Read-only verification of the applied combat setup."""
import json
from pathlib import Path
import unreal

report = {"montages": {}, "abilities": {}, "characters": {}, "misc": {}, "errors": []}
repair = unreal.DTStructRepairLibrary

MONTAGES = [
    "/Game/DTIntegration/Animation/Combat/DT_Standing_1H_Cast_Spell_01_UEFN",
    "/Game/DTIntegration/Animation/Combat/DT_Standing_Melee_Attack_Horizontal_UEFN",
    "/Game/DTIntegration/Animation/Combat/DT_Standing_Melee_Attack_360_High_UEFN",
    "/Game/DTIntegration/Animation/Combat/DT_AMAI_ReactHitFont_UEFN",
    "/Game/DTIntegration/Animation/Combat/DT_Sword_And_Shield_Slash_mixamo_com_Montage_UEFN",
    "/Game/Art/3D/Player/Animations/Anims/Retargeted/Sword_And_Shield_Slash_mixamo_com_Montage",
]
for p in MONTAGES:
    if unreal.EditorAssetLibrary.does_asset_exist(p):
        report["montages"][p] = repair.describe_montage_notifies(p)
    else:
        report["montages"][p] = "MISSING"

def safe(fn, *a, **k):
    try:
        return fn(*a, **k)
    except Exception as exc:
        return "ERR:" + str(exc)

for p in ["/Game/Data/Abilities/Player/DA_Player_FireBolt",
          "/Game/Data/Abilities/Player/DA_Player_IceShards",
          "/Game/Data/Abilities/Player/DA_Player_BasicSlash",
          "/Game/Data/Abilities/Player/DA_Player_FullSwing",
          "/Game/Data/Abilities/DA_AI_BasicAttack"]:
    da = unreal.load_asset(p)
    if not da:
        report["abilities"][p] = "MISSING"
        continue
    report["abilities"][p] = {
        "ability_id": str(da.get_editor_property("ability_id")),
        "kind": str(da.get_editor_property("kind")),
        "release_tag": str(safe(da.get_editor_property, "release_event_tag")),
        "projectile_class": str(safe(lambda: da.get_editor_property("projectile_class").get_path_name())),
        "release_socket": str(safe(da.get_editor_property, "release_socket")),
    }

for p in ["/Game/Blueprints/Characters/BP_PlayerCharacter",
          "/Game/Blueprints/AI/BP_RPGAICharacter",
          "/Game/Blueprints/AI/BP_AIRangedCharacter"]:
    bp = unreal.load_asset(p)
    if not bp:
        report["characters"][p] = "MISSING"
        continue
    cdo = unreal.get_default_object(bp.generated_class())
    router = cdo.get_editor_property("animation_router")
    anims = {}
    for k, v in router.get_editor_property("combat_animations").items():
        m = v.get_editor_property("montage")
        anims[str(k)] = m.get_path_name() if m else None
    hr = router.get_editor_property("hit_reaction")
    hr_m = hr.get_editor_property("montage") if hr else None
    report["characters"][p] = {
        "status": str(safe(bp.get_editor_property, "status")),
        "combat_animations": anims,
        "hit_reaction": hr_m.get_path_name() if hr_m else None,
        "faction": str(safe(cdo.get_editor_property, "faction_id")),
        "ai_controller": str(safe(cdo.get_editor_property, "ai_controller_class")),
    }

for p in ["/Game/Blueprints/Gameplay/Projectiles/BP_Projectile_FireBolt",
          "/Game/Blueprints/Gameplay/Projectiles/BP_Projectile_IceShards"]:
    bp = unreal.load_asset(p)
    if not bp:
        report["misc"][p] = "MISSING"
        continue
    cdo = unreal.get_default_object(bp.generated_class())
    report["misc"][p] = {
        "status": str(safe(bp.get_editor_property, "status")),
        "trail_fx": str(safe(lambda: cdo.get_editor_property("trail_fx").get_path_name())),
        "impact_fx": str(safe(lambda: cdo.get_editor_property("impact_fx").get_path_name())),
        "impact_sound": str(safe(lambda: cdo.get_editor_property("impact_sound").get_path_name())),
    }

aibp = unreal.load_asset("/Game/Blueprints/AI/BP_RPGAIController")
if aibp:
    cdo = unreal.get_default_object(aibp.generated_class())
    aa = safe(lambda: cdo.get_editor_property("attack_ability").get_path_name())
    report["misc"]["BP_RPGAIController.attack_ability"] = str(aa)

ps = unreal.load_asset("/Game/Blueprints/Framework/BP_PlayerState")
if ps:
    cdo = unreal.get_default_object(ps.generated_class())
    skills = cdo.get_editor_property("skills")
    hotbar = []
    for s in skills.get_editor_property("starter_hotbar"):
        d = s.get_editor_property("definition")
        hotbar.append({"slot": int(s.get_editor_property("slot")),
                       "def": d.get_path_name() if d else None})
    report["misc"]["BP_PlayerState.starter_hotbar"] = hotbar
    report["misc"]["BP_PlayerState.status"] = str(safe(ps.get_editor_property, "status"))

out = Path(unreal.Paths.project_saved_dir()) / "Automation" / "CombatSetupVerify.json"
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(report, indent=2, default=str), encoding="utf-8")
unreal.log("VERIFY_DONE")
unreal.SystemLibrary.quit_editor()
