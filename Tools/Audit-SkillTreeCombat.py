"""Inspect live skill-tree widget contracts, slots, catalogue, and lock input."""
import json
from pathlib import Path
import unreal

report = {"widgets": [], "definitions": [], "slots": [], "errors": []}
paths = ["/Game/UI/SpellMenu/" + n for n in
         ("WBP_SpellMenu", "WBP_MageSpellTree", "WBP_WarriorSpellTree", "WBP_MageTalentTree", "WBP_EquippedSpell_Row")]
paths += ["/Game/UI/HUD/SpellGlobes/SpellBoard/WBP_EquippedRow_Button",
          "/Game/UI/HUD/SpellGlobes/SpellBoard/WBP_SpellGlobe_Button"]
for path in paths:
    try:
        bp = unreal.load_asset(path)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        row = {"asset": path, "status": str(bp.get_editor_property("status")), "widgets": []}
        for entry in unreal.DTStructRepairLibrary.inspect_skill_widgets(path, False):
            data = {"raw": entry}
            name = entry.split(" class=")[0]
            data["name"] = name
            data["class"] = entry.split(" class=")[1].split(" ")[0] if " class=" in entry else ""
            for tag in ("AbilityTag=(TagName=\"", "InputTag=(TagName=\"", "SlotTag=(TagName=\""):
                if tag in entry:
                    data[tag.split("=")[0]] = entry.split(tag)[1].split("\"")[0]
            row["widgets"].append(data)
        report["widgets"].append(row)
        unreal.DTStructRepairLibrary.dump_blueprint_graph(path)
    except Exception as exc:
        report["errors"].append(path + ": " + str(exc))
ps = unreal.get_default_object(unreal.load_class(None,
    "/Game/Blueprints/Framework/BP_PlayerState.BP_PlayerState_C"))
skills = ps.get_editor_property("skills")
for definition in skills.get_available_definitions():
    report["definitions"].append({prop: str(definition.get_editor_property(prop)) for prop in
        ("ability_id", "maximum_rank", "required_player_level", "prerequisite_abilities", "kind", "requires_target")})
for slot in skills.get_editor_property("starter_hotbar"):
    definition = slot.get_editor_property("definition")
    report["slots"].append({"slot": slot.get_editor_property("slot"), "ability": str(definition.get_editor_property("ability_id"))})
output = Path(unreal.Paths.project_saved_dir()) / "Automation/SkillTreeCombatAudit.json"
output.write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("SKILL_TREE_AUDIT " + json.dumps(report))
