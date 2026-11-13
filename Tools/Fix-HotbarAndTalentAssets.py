"""Repair equip-row slot tags + talent-tree globe tags via the native
UDTStructRepairLibrary designer-widget helpers (WidgetTree is not
exposed to Python in UE 5.8). Idempotent; logs every decision.
"""
import json
import traceback
from pathlib import Path
import unreal

OUT = Path(unreal.Paths.project_saved_dir()) / "Automation" / "HotbarTalentAssetRepair.json"
report = {"equipped_row_before": [], "equipped_row_fixes": {},
          "talent_tree_before": [], "talent_fixes": {},
          "mage_spell_tree_before": [], "warrior_spell_tree_before": [],
          "errors": []}

ROW = "/Game/UI/SpellMenu/WBP_EquippedSpell_Row"
MAGE_TALENT = "/Game/UI/SpellMenu/WBP_MageTalentTree"
MAGE_SPELL = "/Game/UI/SpellMenu/WBP_MageSpellTree"
WARRIOR_SPELL = "/Game/UI/SpellMenu/WBP_WarriorSpellTree"

SLOT_TAGS = {
    "Spell_LMB": "InputTag.LMB", "Spell_RMB": "InputTag.RMB",
    **{f"Spell_{i}": f"InputTag.{i:02d}" for i in range(1, 10)},
    **{f"Passive_{i}": f"InputTag.Passive.{i}" for i in range(1, 11)},
}

TALENT_HINTS = {
    "halo": "Abilities.Passive.HaloOfProtection",
    "protection": "Abilities.Passive.HaloOfProtection",
    "lifesiphon": "Abilities.Passive.LifeSiphon",
    "siphonlife": "Abilities.Passive.LifeSiphon",
    "manasiphon": "Abilities.Passive.ManaSiphon",
    "siphonmana": "Abilities.Passive.ManaSiphon",
}

try:
    lib = unreal.DTStructRepairLibrary

    report["equipped_row_before"] = [str(x) for x in lib.inspect_skill_widgets(ROW)]
    for widget, tag in SLOT_TAGS.items():
        ok = lib.set_designer_widget_gameplay_tag(ROW, widget, "InputTag", tag)
        report["equipped_row_fixes"][widget] = f"{tag} -> {ok}"

    for label, path in (("mage_spell_tree_before", MAGE_SPELL),
                        ("warrior_spell_tree_before", WARRIOR_SPELL),
                        ("talent_tree_before", MAGE_TALENT)):
        try:
            report[label] = [str(x) for x in lib.inspect_skill_widgets(path)]
        except Exception as e:
            report[label] = [f"ERR {e}"]

    # Only fill still-empty globe tags; never clobber configured ones.
    for line in report["talent_tree_before"]:
        if not isinstance(line, str) or "Globe" not in line and "Talent" not in line:
            continue
        name = line.split()[0]
        lname = name.lower()
        for hint, tag in TALENT_HINTS.items():
            if hint in lname and ("AbilityTag=None" in line or "AbilityTag=" not in line or 'AbilityTag=""' in line):
                ok = lib.set_designer_widget_gameplay_tag(MAGE_TALENT, name, "AbilityTag", tag)
                report["talent_fixes"][name] = f"{tag} -> {ok}"
                break

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("ASSET_REPAIR " + json.dumps(report))
except Exception:
    report["errors"].append(traceback.format_exc())
    try:
        OUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    except Exception:
        pass
    unreal.log_error(traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
