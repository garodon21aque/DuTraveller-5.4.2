"""Fresh audit: compile status, cast targets and key call nodes for the
hotbar/skill/equipment UI chain plus framework class defaults.

Reports machine-readable AUDIT lines; never saves anything.
"""
import json
from pathlib import Path
import unreal

WIDGETS = [
    "/Game/UI/InventorySystem/WBP_Equipment",
    "/Game/UI/InventorySystem/WBP_EquipmentSlot",
    "/Game/UI/InventorySystem/WBP_EquipmentToolBar",
    "/Game/UI/InventorySystem/WBP_PlayerInventory",
    "/Game/UI/InventorySystem/WBP_ItemObject",
    "/Game/UI/InventorySystem/WBP_DropWidget",
    "/Game/UI/InventorySystem/WBP_InvItemTooltip",
    "/Game/UI/InventorySystem/WBP_PlayerStat",
    "/Game/UI/HUD/WBP_HUD",
    "/Game/UI/HUD/WBP_HealthManaSpells",
    "/Game/UI/HUD/WBP_PlayerSkill",
    "/Game/UI/HUD/WBP_Skills",
    "/Game/UI/HUD/WBP_SkillActions",
    "/Game/UI/HUD/WBP_Trainer",
    "/Game/UI/HUD/SpellGlobes/WBP_SpellGlobe",
    "/Game/UI/HUD/SpellGlobes/SpellBoard/WBP_EquippedRow_Button",
    "/Game/UI/HUD/SpellGlobes/SpellBoard/WBP_SpellGlobe_Button",
    "/Game/UI/SpellMenu/WBP_SpellMenu",
    "/Game/UI/SpellMenu/WBP_EquippedSpell_Row",
    "/Game/UI/SpellMenu/WBP_MageSpellTree",
    "/Game/UI/SpellMenu/WBP_WarriorSpellTree",
    "/Game/UI/SpellMenu/WBP_MageTalentTree",
    "/Game/UI/Attribute/WBP_AttributesPointsRow",
    "/Game/UI/InventorySystem/WBP_NPCVendorShop",
    "/Game/UI/InventorySystem/WBP_NPCVendorShopSell",
    "/Game/UI/Menu/WBP_PauseMenu",
    "/Game/UI/QuestSystem/WBP_QuestGiver",
    "/Game/UI/QuestSystem/WBP_QuestRewards",
    "/Game/UI/CraftingSystem/WBP_ItemBP",
]

CHARACTERS = [
    "/Game/Blueprints/Characters/BP_PlayerCharacter",
    "/Game/Blueprints/Player/BP_PlayerCharacter",
    "/Game/Blueprints/Player/BP_RPGCharacter",
]

report = {"widgets": [], "characters": [], "defaults": {}}

def probe(path):
    asset = unreal.load_asset(path)
    if not asset:
        return {"asset": path, "missing": True}
    entry = {"asset": path}
    try:
        parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(asset)
        entry["parent"] = parent.get_path_name() if parent else None
    except Exception as exc:
        entry["parent_error"] = str(exc)
    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(asset)
        entry["status"] = str(asset.get_editor_property("status"))
    except Exception as exc:
        entry["status"] = "CompileException: " + str(exc)
    try:
        cdo = unreal.get_default_object(asset.generated_class())
        funcs = []
        for name in ("equipment_appearance_changed", "initialize_player_captures",
                     "activate_hotbar_slot"):
            try:
                func = cdo.get_class().find_function(name)
                if func:
                    funcs.append(name + "@" + (func.get_outer().get_name() if func.get_outer() else "?"))
            except Exception:
                pass
        entry["overrides"] = funcs
    except Exception as exc:
        entry["cdo_error"] = str(exc)
    return entry

try:
    for path in WIDGETS + CHARACTERS:
        report["widgets" if path.startswith("/Game/UI") else "characters"].append(probe(path))
except Exception as exc:
    unreal.log_error("probe phase failed: " + str(exc))

try:
    # Framework defaults that gate the hotbar input chain.
    pc_bp = unreal.load_asset("/Game/Blueprints/Framework/BP_PlayerController")
    pc = unreal.get_default_object(pc_bp.generated_class())
    for name in ("b_enable_combat_hotbar_input", "enable_combat_hotbar_input"):
        try:
            report["defaults"]["hotbar_input_flag"] = {name: pc.get_editor_property(name)}
            break
        except Exception:
            continue

    gm = unreal.get_default_object(
        unreal.load_asset("/Game/Blueprints/Framework/BP_RPGGameModeBase").generated_class())
    report["defaults"]["gm_pawn"] = gm.get_editor_property("default_pawn_class").get_path_name()
    report["defaults"]["gm_pc"] = gm.get_editor_property("player_controller_class").get_path_name()
    report["defaults"]["gm_ps"] = gm.get_editor_property("player_state_class").get_path_name()

    # Ability definitions as actually configured.
    ps_bp = unreal.load_asset("/Game/Blueprints/Framework/BP_PlayerState")
    ps = unreal.get_default_object(ps_bp.generated_class())
    skills = ps.get_editor_property("skills")
    hotbar = []
    for entry_obj in skills.get_editor_property("starter_hotbar"):
        hotbar.append({"slot": entry_obj.get_editor_property("slot"),
                       "def": entry_obj.get_editor_property("definition").get_path_name()
                       if entry_obj.get_editor_property("definition") else None})
    report["defaults"]["starter_hotbar"] = hotbar
    report["defaults"]["available_abilities"] = [
        d.get_path_name() for d in skills.get_editor_property("available_abilities") if d]
    cat = skills.get_editor_property("ability_catalogue")
    report["defaults"]["ability_catalogue"] = cat.get_path_name() if cat else None
except Exception as exc:
    unreal.log_error("defaults phase failed: " + str(exc))

try:
    for path in WIDGETS:
        unreal.DTStructRepairLibrary.dump_blueprint_graph(path)
    for path in CHARACTERS:
        unreal.DTStructRepairLibrary.dump_blueprint_graph(path)
except Exception as exc:
    unreal.log_error("dump phase failed: " + str(exc))
finally:
    out = Path(unreal.Paths.project_saved_dir()) / "Automation" / "HotbarEquipmentAudit.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("AUDIT_JSON " + json.dumps(report))
    unreal.SystemLibrary.quit_editor()
