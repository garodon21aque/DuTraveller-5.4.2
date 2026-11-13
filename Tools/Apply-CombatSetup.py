"""Wire the Fireball/melee runtime: UEFN retargets, authored notifies, projectile
Blueprints, ability-definition release data, router combat animations, AI config.

Every step is idempotent; assets are only resaved when actually changed.
Writes Saved/Automation/CombatSetupApply.json.
"""
import json
from pathlib import Path
import traceback
import unreal

report = {"retargeted": [], "notifies": [], "blueprints": [], "assets": [],
          "checks": [], "errors": []}
tools = unreal.AssetToolsHelpers.get_asset_tools()

def safe(fn, *a, **k):
    try:
        return fn(*a, **k)
    except Exception as exc:
        return "ERR:" + str(exc)

def log_check(name, ok, detail=""):
    report["checks"].append({"check": name, "ok": bool(ok), "detail": str(detail)})
    unreal.log("CHECK %s = %s %s" % (name, ok, detail))

def save(asset, label):
    if unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
        report["assets"].append(asset.get_path_name())
        return True
    report["errors"].append("save failed: " + label)
    return False

# ------------------------------------------------------------------ 1. retarget
RETARGET = [
    ("/Game/Art/3D/Player/Animations/Anims/Retargeted/Standing_1H_Cast_Spell_01_mixamo_com_Montage",
     "DT_Standing_1H_Cast_Spell_01_UEFN"),
    ("/Game/Art/3D/Player/Animations/Anims/Retargeted/Standing_Melee_Attack_Horizontal_mixamo_com_Montage",
     "DT_Standing_Melee_Attack_Horizontal_UEFN"),
    ("/Game/Art/3D/Player/Animations/Anims/Retargeted/Standing_Melee_Attack_360_High_mixamo_com_Montage",
     "DT_Standing_Melee_Attack_360_High_UEFN"),
    ("/Game/Art/3D/Ai/Enemy/Anims/AMAI_ReactHitFont",
     "DT_AMAI_ReactHitFont_UEFN"),
]
UEFN_TARGET = "/Game/DTIntegration/Animation/Combat"
source_mesh = unreal.load_asset("/Game/Characters/Mannequin_UE4/Meshes/SK_Mannequin")
target_mesh = unreal.load_asset("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin")
retargeter = unreal.load_asset("/Game/DTIntegration/Animation/RTG_DT_LegacyUE4_UEFN")
target_skeleton = target_mesh.get_editor_property("skeleton")
retargeted = {}

for src_path, out_name in RETARGET:
    dest = UEFN_TARGET + "/" + out_name
    if unreal.EditorAssetLibrary.does_asset_exist(dest):
        retargeted[out_name] = dest
        continue
    if not unreal.EditorAssetLibrary.does_asset_exist(src_path):
        report["errors"].append("retarget source missing: " + src_path)
        continue
    inputs = unreal.IKRetargetBatchOperationInputs()
    for prop, value in dict(
            assets_to_retarget=[unreal.EditorAssetLibrary.find_asset_data(src_path)],
            source_mesh=source_mesh, target_mesh=target_mesh,
            ik_retarget_asset=retargeter, prefix="", suffix="",
            target_path=UEFN_TARGET, include_referenced_assets=True,
            overwrite_existing_files=False).items():
        inputs.set_editor_property(prop, value)
    try:
        outputs = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    except Exception as exc:
        report["errors"].append("retarget failed %s: %s" % (src_path, exc))
        continue
    produced = None
    for data in outputs:
        asset = data.get_asset()
        if isinstance(asset,unreal.AnimMontage) and asset.get_editor_property("skeleton") == target_skeleton:
            produced = asset
            break
    if not produced:
        report["errors"].append("retarget produced no UEFN asset for " + src_path)
        continue
    if produced.get_path_name().split(".")[0] != dest:
        new_name = unreal.EditorAssetLibrary.rename_asset(produced.get_path_name().split(".")[0], dest)
        if new_name:
            produced = unreal.load_asset(dest)
    save(produced, "retarget " + out_name)
    retargeted[out_name] = produced.get_path_name().split(".")[0]
    report["retargeted"].append(produced.get_path_name().split(".")[0])

for name in [n for _, n in RETARGET]:
    dest = UEFN_TARGET + "/" + name
    if name not in retargeted and unreal.EditorAssetLibrary.does_asset_exist(dest):
        retargeted[name] = dest

# ------------------------------------------------------------------ 2. notifies
repair = unreal.DTStructRepairLibrary

def add_event(path, tag, time_s):
    if not path or not unreal.EditorAssetLibrary.does_asset_exist(path):
        report["errors"].append("notify target missing: " + str(path))
        return
    for line in repair.describe_montage_notifies(path):
        unreal.log(line)
    if repair.add_montage_gameplay_event_notify(path, tag, time_s):
        report["notifies"].append({"asset": path, "event": tag, "time": time_s})
    else:
        report["errors"].append("event notify failed: " + path)

def add_window(path, start, end):
    if not path or not unreal.EditorAssetLibrary.does_asset_exist(path):
        report["errors"].append("notify target missing: " + str(path))
        return
    existing = repair.describe_montage_notifies(path)
    if any("DTAnimNotifyMeleeWindow" in line for line in existing):
        return
    if repair.add_montage_melee_window(path, start, end):
        report["notifies"].append({"asset": path, "window": [start, end]})
    else:
        report["errors"].append("melee window failed: " + path)

cast_uefn = retargeted.get("DT_Standing_1H_Cast_Spell_01_UEFN")
horiz_uefn = retargeted.get("DT_Standing_Melee_Attack_Horizontal_UEFN")
swing360_uefn = retargeted.get("DT_Standing_Melee_Attack_360_High_UEFN")
react_uefn = retargeted.get("DT_AMAI_ReactHitFont_UEFN")
slash_uefn = "/Game/DTIntegration/Animation/Combat/DT_Sword_And_Shield_Slash_mixamo_com_Montage_UEFN"

if cast_uefn and "SpellRelease" not in " ".join(repair.describe_montage_notifies(cast_uefn)):
    add_event(cast_uefn, "Event.Animation.SpellRelease", 1.25)
add_window(slash_uefn, 0.45, 0.95)
add_window(horiz_uefn, 0.35, 0.75)
add_window(swing360_uefn, 1.15, 2.05)
# UE4-skeleton melee montage for AI characters whose meshes use the legacy skeleton.
add_window("/Game/Art/3D/Player/Animations/Anims/Retargeted/Sword_And_Shield_Slash_mixamo_com_Montage", 0.45, 0.95)

# ------------------------------------------------------------------ 3. projectile BPs
PROJ_DIR = "/Game/Blueprints/Gameplay/Projectiles"
projectile_class = unreal.load_class(None, "/Script/DuTraveller.RPGProjectile")

def create_projectile_bp(name, trail_fx, impact_fx, impact_sound):
    path = PROJ_DIR + "/" + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", projectile_class)
        bp = tools.create_asset(name, PROJ_DIR, unreal.Blueprint, factory)
        if not bp:
            report["errors"].append("could not create " + path)
            return None
        report["blueprints"].append(path)
    else:
        bp = unreal.load_asset(path)
    cdo = unreal.get_default_object(bp.generated_class())
    if trail_fx and unreal.EditorAssetLibrary.does_asset_exist(trail_fx):
        cdo.set_editor_property("trail_fx", unreal.load_asset(trail_fx))
    if impact_fx and unreal.EditorAssetLibrary.does_asset_exist(impact_fx):
        cdo.set_editor_property("impact_fx", unreal.load_asset(impact_fx))
    if impact_sound and unreal.EditorAssetLibrary.does_asset_exist(impact_sound):
        cdo.set_editor_property("impact_sound", unreal.load_asset(impact_sound))
    save(bp, name)
    return bp

FIRE_TRAIL = "/Game/Art/Niagara/Spells/FIre/FireBolt"
FIRE_SOUND = "/Game/Audio/Meta/MetaSounds/SFX_Projectile"
fire_bp = create_projectile_bp("BP_Projectile_FireBolt", FIRE_TRAIL, "/Game/Art/Niagara/Explode/Fire_Burst", FIRE_SOUND)
ice_bp = create_projectile_bp("BP_Projectile_IceShards", None, None, None)

# ------------------------------------------------------------------ 4. ability data
def set_props(path, props):
    da = unreal.load_asset(path)
    if not da:
        report["errors"].append("missing data asset: " + path)
        return
    changed = False
    for key, value in props.items():
        try:
            if da.get_editor_property(key) != value:
                da.set_editor_property(key, value)
                changed = True
        except Exception as exc:
            report["errors"].append("%s.%s: %s" % (path, key, exc))
    if changed:
        save(da, path)

def cls_of(bp):
    return bp.generated_class() if bp else None

for da_path, bp in (("/Game/Data/Abilities/Player/DA_Player_FireBolt", fire_bp),
                    ("/Game/Data/Abilities/Player/DA_Player_IceShards", ice_bp)):
    if not bp:
        report["errors"].append("no projectile blueprint for " + da_path)
        continue
    cls_path = bp.generated_class().get_path_name()
    if repair.configure_ability_release(da_path, "Event.Animation.SpellRelease",
                                        cls_path, "hand_r"):
        report["assets"].append(da_path + " release-config")
    else:
        report["errors"].append("release config failed: " + da_path)

# ------------------------------------------------------------------ 5. combat animations
def anim_entry(montage_path):
    entry = unreal.DTCombatAnimation()
    entry.set_editor_property("montage", unreal.load_asset(montage_path))
    entry.set_editor_property("start_section", "")
    entry.set_editor_property("play_rate", 1.0)
    return entry

def set_router_map(char_path, mapping, hit_reaction=None):
    bp = unreal.load_asset(char_path)
    if not bp:
        report["errors"].append("missing character: " + char_path)
        return
    cdo = unreal.get_default_object(bp.generated_class())
    router = cdo.get_editor_property("animation_router")
    if not router:
        report["errors"].append("no animation router on " + char_path)
        return
    current = dict(router.get_editor_property("combat_animations"))
    changed = False
    for ability_id, montage_path in mapping.items():
        if not montage_path or not unreal.EditorAssetLibrary.does_asset_exist(montage_path):
            report["errors"].append("montage missing for %s: %s" % (ability_id, montage_path))
            continue
        existing = current.get(ability_id)
        current_montage = existing.get_editor_property("montage") if existing else None
        want = unreal.load_asset(montage_path)
        if current_montage != want:
            current[ability_id] = anim_entry(montage_path)
            changed = True
    if changed:
        router.set_editor_property("combat_animations", current)
    if hit_reaction and unreal.EditorAssetLibrary.does_asset_exist(hit_reaction):
        hr = router.get_editor_property("hit_reaction")
        if not hr or hr.get_editor_property("montage") != unreal.load_asset(hit_reaction):
            router.set_editor_property("hit_reaction", anim_entry(hit_reaction))
            changed = True
    if changed:
        save(bp, char_path)
        report["assets"].append(char_path + " router-map")

set_router_map("/Game/Blueprints/Characters/BP_PlayerCharacter", {
    "Abilities.Fire.FireBolt": cast_uefn,
    "Abilities.Ice.IceShards": cast_uefn,
    "Abilities.Melee.Slash": horiz_uefn,
    "Abilities.Melee.FullSwing": swing360_uefn,
}, hit_reaction=react_uefn)

UE4_SLASH = "/Game/Art/3D/Player/Animations/Anims/Retargeted/Sword_And_Shield_Slash_mixamo_com_Montage"
for ai_path in ["/Game/Blueprints/AI/BP_RPGAICharacter", "/Game/Blueprints/AI/BP_AIRangedCharacter"]:
    set_router_map(ai_path, {"AI.BasicAttack": UE4_SLASH})

# ------------------------------------------------------------------ 6. AI controller
ai_bp = unreal.load_asset("/Game/Blueprints/AI/BP_RPGAIController")
if ai_bp:
    parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(ai_bp)
    cdo = unreal.get_default_object(ai_bp.generated_class())
    aa = unreal.load_asset("/Game/Data/Abilities/DA_AI_BasicAttack")
    if aa and safe(cdo.get_editor_property, "attack_ability") != aa:
        cdo.set_editor_property("attack_ability", aa)
        save(ai_bp, "BP_RPGAIController")
    report["ai_controller_parent"] = str(parent)

# ------------------------------------------------------------------ 7. starter hotbar
ps_bp = unreal.load_asset("/Game/Blueprints/Framework/BP_PlayerState")
if ps_bp:
    cdo = unreal.get_default_object(ps_bp.generated_class())
    skills = cdo.get_editor_property("skills")
    wanted = [(0, "DA_Player_BasicSlash"), (1, "DA_Player_FireBolt"),
              (2, "DA_Player_FireBolt"), (3, "DA_Player_FullSwing"),
              (4, "DA_Player_IceShards")]
    new_hotbar = []
    for slot, name in wanted:
        da = unreal.load_asset("/Game/Data/Abilities/Player/" + name)
        if not da:
            continue
        entry = unreal.RPGHotbarSlot()
        entry.set_editor_property("slot", slot)
        entry.set_editor_property("definition", da)
        new_hotbar.append(entry)
    if new_hotbar:
        skills.set_editor_property("starter_hotbar", new_hotbar)
        save(ps_bp, "BP_PlayerState")

out = Path(unreal.Paths.project_saved_dir()) / "Automation" / "CombatSetupApply.json"
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(report, indent=2, default=str), encoding="utf-8")
unreal.log("APPLY_DONE errors=%d" % len(report["errors"]))
for e in report["errors"]:
    unreal.log("APPLY_ERR " + e)
unreal.SystemLibrary.quit_editor()
