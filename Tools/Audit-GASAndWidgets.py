"""Deep audit: ability definitions, tag usage, widget trees, components and
graph dumps for the combat/UI repair pass. Read-only; never saves assets."""
import json
import traceback
from pathlib import Path
import unreal

OUT = Path(unreal.Paths.project_saved_dir()) / "Automation" / "GASWidgetAudit-20261103.json"

ABILITY_ASSETS = [
    "/Game/Data/Abilities/Player/DA_Player_AirBolt",
    "/Game/Data/Abilities/Player/DA_Player_BasicSlash",
    "/Game/Data/Abilities/Player/DA_Player_ChainLightning",
    "/Game/Data/Abilities/Player/DA_Player_EarthQuake",
    "/Game/Data/Abilities/Player/DA_Player_EarthWall",
    "/Game/Data/Abilities/Player/DA_Player_ElectricBlink",
    "/Game/Data/Abilities/Player/DA_Player_ElectricBolt",
    "/Game/Data/Abilities/Player/DA_Player_FireBlast",
    "/Game/Data/Abilities/Player/DA_Player_FireBolt",
    "/Game/Data/Abilities/Player/DA_Player_FireBreath",
    "/Game/Data/Abilities/Player/DA_Player_FullSwing",
    "/Game/Data/Abilities/Player/DA_Player_HaloOfProtection",
    "/Game/Data/Abilities/Player/DA_Player_HolyShield",
    "/Game/Data/Abilities/Player/DA_Player_HolySmite",
    "/Game/Data/Abilities/Player/DA_Player_IceShards",
    "/Game/Data/Abilities/Player/DA_Player_IceSpikes",
    "/Game/Data/Abilities/Player/DA_Player_IceStorm",
    "/Game/Data/Abilities/Player/DA_Player_IceWall",
    "/Game/Data/Abilities/Player/DA_Player_LifeSiphon",
    "/Game/Data/Abilities/Player/DA_Player_LightBolt",
    "/Game/Data/Abilities/Player/DA_Player_LightHeal",
    "/Game/Data/Abilities/Player/DA_Player_ManaSiphon",
    "/Game/Data/Abilities/Player/DA_Player_MeteorStrike",
    "/Game/Data/Abilities/Player/DA_Player_PushBack",
    "/Game/Data/Abilities/Player/DA_Player_Root",
    "/Game/Data/Abilities/Player/DA_Player_ShadowBolt",
    "/Game/Data/Abilities/Player/DA_Player_ShadowFear",
    "/Game/Data/Abilities/Player/DA_Player_ShadowShipon",
    "/Game/Data/Abilities/Player/DA_Player_Swing",
    "/Game/Data/Abilities/Player/DA_Player_WaterBolt",
    "/Game/Data/Abilities/Player/DA_Player_WaterHeal",
    "/Game/Data/Abilities/Player/DA_Player_WaterStream",
    "/Game/Data/Abilities/DA_AI_BasicAttack",
    "/Game/Data/Abilities/Enemies/DA_CombatTest_FireBolt",
    "/Game/Data/Abilities/Enemies/DA_CombatTest_Slash",
]

WIDGET_TREES = [
    "/Game/UI/HUD/SpellGlobes/WBP_SpellGlobe",
    "/Game/UI/HUD/SpellGlobes/SpellBoard/WBP_SpellGlobe_Button",
    "/Game/UI/HUD/SpellGlobes/SpellBoard/WBP_EquippedRow_Button",
    "/Game/UI/HUD/WBP_HealthManaSpells",
    "/Game/UI/HUD/WBP_HUD",
    "/Game/UI/HUD/WBP_Skills",
    "/Game/UI/HUD/WBP_PlayerSkill",
    "/Game/UI/HUD/WBP_SkillActions",
    "/Game/UI/World/WBP_ItemInteract",
    "/Game/UI/World/WBP_DMGPopup",
    "/Game/UI/World/WBP_DefaultInteraction",
    "/Game/UI/World/WBP_EnemyHealthWidget",
    "/Game/UI/World/WPB_TargetEnemy",
    "/Game/UI/World/WBP_EffectContainer",
    "/Game/UI/World/WBP_EffectSlot",
    "/Game/UI/World/WBP_OnPawnSeen",
    "/Game/UI/World/WBP_XPPopUp",
    "/Game/UI/SpellMenu/WBP_MageTalentTree",
    "/Game/UI/SpellMenu/WBP_SpellMenu",
    "/Game/UI/SpellMenu/WBP_EquippedSpell_Row",
    "/Game/UI/SpellMenu/WBP_MageSpellTree",
    "/Game/UI/SpellMenu/WBP_WarriorSpellTree",
]

BP_COMPONENTS = [
    "/Game/Blueprints/Actors/Interactables/Items/BP_MasterPickupItem",
    "/Game/Blueprints/Actors/Interactables/Items/BP_Pickup",
    "/Game/UI/World/BP_DamageTextComponent",
    "/Game/Blueprints/Characters/BP_PlayerCharacter",
    "/Game/Blueprints/Framework/BP_PlayerController",
    "/Game/Blueprints/Framework/BP_PlayerState",
    "/Game/Blueprints/Gameplay/Projectiles/BP_Projectile_FireBolt",
    "/Game/Blueprints/Gameplay/Projectiles/BP_Projectile_ElectricBolt",
    "/Game/Blueprints/Gameplay/Projectiles/BP_Projectile_AirBolt",
    "/Game/Blueprints/Gameplay/Projectiles/BP_Projectile_IceShards",
    "/Game/Blueprints/Gameplay/Projectiles/BP_Projectile_LightBolt",
    "/Game/Blueprints/Gameplay/Projectiles/BP_Projectile_ShadowBolt",
    "/Game/Blueprints/Gameplay/Projectiles/BP_Projectile_WaterBolt",
]

GRAPH_DUMPS = [
    "/Game/UI/HUD/SpellGlobes/WBP_SpellGlobe",
    "/Game/UI/HUD/SpellGlobes/SpellBoard/WBP_SpellGlobe_Button",
    "/Game/UI/HUD/SpellGlobes/SpellBoard/WBP_EquippedRow_Button",
    "/Game/UI/HUD/WBP_HealthManaSpells",
    "/Game/UI/HUD/WBP_HUD",
    "/Game/UI/HUD/WBP_Skills",
    "/Game/UI/HUD/WBP_PlayerSkill",
    "/Game/UI/HUD/WBP_SkillActions",
    "/Game/UI/World/WBP_ItemInteract",
    "/Game/UI/World/WBP_DMGPopup",
    "/Game/UI/World/BP_DamageTextComponent",
    "/Game/UI/World/WBP_DefaultInteraction",
    "/Game/UI/World/WBP_EnemyHealthWidget",
    "/Game/UI/World/WPB_TargetEnemy",
    "/Game/UI/World/WBP_OnPawnSeen",
    "/Game/Blueprints/Actors/Interactables/Items/BP_MasterPickupItem",
    "/Game/Blueprints/Actors/Interactables/Items/BP_Pickup",
    "/Game/UI/SpellMenu/WBP_MageTalentTree",
    "/Game/UI/SpellMenu/WBP_SpellMenu",
    "/Game/UI/SpellMenu/WBP_EquippedSpell_Row",
]

report = {"abilities": [], "datatable": [], "widget_trees": {}, "bp_components": {},
          "defaults": {}, "referencers": {}, "errors": []}


def safe_str(value):
    try:
        if value is None:
            return None
        if isinstance(value, (bool, int, float, str)):
            return value
        if hasattr(value, "get_path_name"):
            return value.get_path_name()
        return str(value)
    except Exception:
        return "<unreadable>"


def dump_props(obj, names):
    row = {}
    for name in names:
        try:
            row[name] = safe_str(obj.get_editor_property(name))
        except Exception:
            row[name] = "<absent>"
    return row


def walk_widget(widget, depth=0, acc=None):
    if acc is None:
        acc = []
    if not widget:
        return acc
    entry = {"depth": depth, "name": widget.get_name(), "cls": widget.get_class().get_name()}
    try:
        entry["visibility"] = str(widget.get_editor_property("visibility"))
    except Exception:
        pass
    acc.append(entry)
    try:
        for slot in widget.get_editor_property("slots") or []:
            try:
                walk_widget(slot.content, depth + 1, acc)
            except Exception:
                pass
    except Exception:
        pass
    return acc


def dump_bp_components(bp_path):
    entry = {"components": [], "vars": {}}
    bp = unreal.load_asset(bp_path)
    if not bp:
        return {"missing": True}
    try:
        scs = bp.get_editor_property("simple_construction_script")
        if scs:
            for node in scs.get_all_nodes():
                comp = node.get_editor_property("component_template")
                if not comp:
                    continue
                crow = {"name": comp.get_name(), "cls": comp.get_class().get_name()}
                for prop in ("widget_class", "space", "draw_size", "static_mesh",
                             "widget", "component_class"):
                    try:
                        crow[prop] = safe_str(comp.get_editor_property(prop))
                    except Exception:
                        pass
                entry["components"].append(crow)
    except Exception:
        entry["components_error"] = traceback.format_exc()
    try:
        cdo = unreal.get_default_object(bp.generated_class())
        for name in dir(cdo):
            if name.startswith(("get_", "set_", "on_", "k2_", "receive_")):
                continue
            try:
                value = cdo.get_editor_property(name)
            except Exception:
                continue
            sval = safe_str(value)
            if sval is not None and sval != "<unreadable>":
                entry["vars"][name] = sval
    except Exception:
        entry["vars_error"] = traceback.format_exc()
    return entry


try:
    # ---- Ability definitions -------------------------------------------------
    prop_names = ["ability_id", "display_name", "kind", "execution", "maximum_rank",
                  "required_player_level", "spell_point_cost_per_rank",
                  "prerequisite_abilities", "base_damage_by_rank", "attack_power_scale",
                  "ability_power_scale", "mana_cost", "stamina_cost", "range", "cooldown",
                  "b_requires_target", "requires_target", "b_allow_friendly_fire",
                  "allow_friendly_fire", "action_class", "release_event_tag",
                  "projectile_class", "release_socket", "damage_effect",
                  "projectile_speed", "effect_radius", "effect_duration",
                  "effect_magnitude", "release_fx", "presentation"]
    for path in ABILITY_ASSETS:
        asset = unreal.load_asset(path)
        if not asset:
            report["abilities"].append({"asset": path, "missing": True})
            continue
        row = dump_props(asset, prop_names)
        row["asset"] = path
        pres = row.get("presentation")
        if pres and not isinstance(pres, str):
            try:
                row["presentation"] = dump_props(
                    asset.get_editor_property("presentation"),
                    ["ability_tag", "input_tag", "status_tag", "cooldown_tag",
                     "ability_type", "icon", "background_material", "ability",
                     "level_requirement"])
            except Exception:
                row["presentation"] = str(pres)
        report["abilities"].append(row)

    # ---- Catalogue DataTable --------------------------------------------------
    dt = unreal.load_asset("/Game/Data/Catalogues/DT_PlayerAbilities")
    if dt:
        try:
            for row_name in dt.get_row_names():
                row = dt.find_row(str(row_name), "audit")
                d = row.get_editor_property("definition") if row else None
                report["datatable"].append({"row": str(row_name),
                                            "definition": d.get_path_name() if d else None})
        except Exception:
            report["errors"].append("datatable: " + traceback.format_exc())

    # ---- Widget trees ----------------------------------------------------------
    for path in WIDGET_TREES:
        bp = unreal.load_asset(path)
        if not bp:
            report["widget_trees"][path] = {"missing": True}
            continue
        tree = {"root": None, "widgets": [], "vars": {}}
        try:
            wt = bp.get_editor_property("widget_tree")
            root = wt.get_editor_property("root_widget") if wt else None
            tree["widgets"] = walk_widget(root)
        except Exception:
            tree["tree_error"] = traceback.format_exc()
        try:
            cdo = unreal.get_default_object(bp.generated_class())
            for name in dir(cdo):
                if name.startswith(("get_", "set_", "on_", "k2_", "receive_", "widget_")):
                    continue
                try:
                    value = cdo.get_editor_property(name)
                except Exception:
                    continue
                sval = safe_str(value)
                if sval is not None and sval != "<unreadable>":
                    tree["vars"][name] = sval
        except Exception:
            tree["vars_error"] = traceback.format_exc()
        report["widget_trees"][path] = tree

    # ---- Blueprint components/defaults -----------------------------------------
    for path in BP_COMPONENTS:
        report["bp_components"][path] = dump_bp_components(path)

    # ---- Framework defaults ------------------------------------------------------
    try:
        ps_bp = unreal.load_asset("/Game/Blueprints/Framework/BP_PlayerState")
        ps = unreal.get_default_object(ps_bp.generated_class())
        skills = ps.get_editor_property("skills")
        report["defaults"]["starter_hotbar"] = [
            {"slot": e.get_editor_property("slot"),
             "def": safe_str(e.get_editor_property("definition"))}
            for e in skills.get_editor_property("starter_hotbar")]
        report["defaults"]["ability_catalogue"] = safe_str(skills.get_editor_property("ability_catalogue"))
        report["defaults"]["hotbar_size"] = skills.get_editor_property("hotbar_size")
        report["defaults"]["available_abilities"] = [
            safe_str(d) for d in skills.get_editor_property("available_abilities") if d]
    except Exception:
        report["errors"].append("playerstate: " + traceback.format_exc())
    try:
        pc_bp = unreal.load_asset("/Game/Blueprints/Framework/BP_PlayerController")
        pc = unreal.get_default_object(pc_bp.generated_class())
        report["defaults"]["hotbar_keys"] = {str(k): v for k, v in
            pc.get_editor_property("hotbar_keys").items()}
        report["defaults"]["b_enable_combat_hotbar_input"] = pc.get_editor_property("b_enable_combat_hotbar_input")
    except Exception:
        report["errors"].append("playercontroller: " + traceback.format_exc())

    # ---- Referencers of the world widgets ----------------------------------------
    try:
        reg = unreal.AssetRegistryHelpers.get_asset_registry()
        opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,
                                                     include_hard_package_references=True,
                                                     include_searchable_names=False,
                                                     include_soft_management_references=False,
                                                     include_hard_management_references=False)
        for target in ("/Game/UI/World/WBP_ItemInteract", "/Game/UI/World/WBP_DMGPopup",
                       "/Game/UI/World/BP_DamageTextComponent",
                       "/Game/UI/World/WBP_EnemyHealthWidget",
                       "/Game/UI/World/WBP_DefaultInteraction"):
            try:
                deps = reg.get_referencers(target, opts)
                report["referencers"][target] = [str(d) for d in deps]
            except Exception:
                report["referencers"][target] = ["error: " + traceback.format_exc().splitlines()[-1]]
    except Exception:
        report["errors"].append("referencers: " + traceback.format_exc())

    # ---- Graph dumps to log -------------------------------------------------------
    for path in GRAPH_DUMPS:
        try:
            unreal.DTStructRepairLibrary.dump_blueprint_graph(path)
        except Exception:
            unreal.log_error("graph dump failed " + path)

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("GAS_WIDGET_AUDIT " + json.dumps(report))
except Exception:
    unreal.log_error(traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
