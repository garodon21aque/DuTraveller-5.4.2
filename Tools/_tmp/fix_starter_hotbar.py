import unreal, json
ps_class = unreal.load_class(None, "/Game/Blueprints/Framework/BP_PlayerState.BP_PlayerState_C")
ps = unreal.get_default_object(ps_class)
skills = ps.get_editor_property("skills")
starters = list(skills.get_editor_property("starter_hotbar"))
out = {"before": [(s.get_editor_property("slot"), str(s.get_editor_property("definition").get_editor_property("ability_id"))) for s in starters]}
seen = {}
fixed = []
for s in starters:
    slot = s.get_editor_property("slot")
    d = s.get_editor_property("definition")
    if d is None or str(d.get_editor_property("ability_id")) in seen:
        replacement = unreal.load_asset("/Game/Data/Abilities/Player/DA_Player_ElectricBolt")
        s.set_editor_property("definition", replacement)
        d = replacement
    seen[str(d.get_editor_property("ability_id"))] = slot
    fixed.append(s)
skills.set_editor_property("starter_hotbar", fixed)
out["after"] = [(s.get_editor_property("slot"), str(s.get_editor_property("definition").get_editor_property("ability_id"))) for s in skills.get_editor_property("starter_hotbar")]
ok = unreal.DTStructRepairLibrary.force_compile_and_save("/Game/Blueprints/Framework/BP_PlayerState")
out["saved"] = ok
unreal.log("STARTER_FIX " + json.dumps(out))
