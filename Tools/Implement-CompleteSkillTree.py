"""Complete the existing widget skill catalogue, preserve legacy assets, and configure tree progression.

Projectile, teleport, heal and defensive patterns use the old local project's
source as reference. Other payloads/prerequisites are new reconstruction design.
Existing player balance is preserved; new balance remains editable in assets.
"""
import json
import re
import shutil
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
tools = unreal.AssetToolsHelpers.get_asset_tools()
report = {"passed": False, "skills": [], "widgets": {}, "errors": [],
          "legacy_reference": "D:/GameJams/Github/DuTraveller",
          "design": "Recovered projectile/teleport/heal/defence patterns with new GAS payloads and explicit prerequisites; no claim of original balance."}
FX = "/Game/Art/Niagara/"
cast_path = "/Game/DTIntegration/Animation/Combat/DT_Standing_1H_Cast_Spell_01_UEFN"
melee_path = "/Game/DTIntegration/Animation/Combat/DT_Standing_Melee_Attack_Horizontal_UEFN"
# branch, leaf, payload, prerequisite leaf, icon, effect, description
specs = [
 ("Fire","FireBolt","PROJECTILE",None,"Fire_Ball","Spells/FIre/FireBolt","Launch a fire projectile at the montage release."),
 ("Fire","FireBlast","AREA","FireBolt","Fire_Chain","Explode/Fire_Burst","Explode at the aimed point, damaging nearby enemies."),
 ("Fire","FireBreath","CONE","FireBlast","dragon-breath","Spells/FIre/Fire_Ball","Release a forward cone of fire."),
 ("Fire","MeteorStrike","AREA","FireBreath","Meteor_Strike","Explode/Fire_Burst","Strike a large area around the aimed point."),
 ("Ice","IceShards","PROJECTILE",None,"Frost_Ball","Spells/Ice/NS_IceShards","Launch an ice projectile at the montage release."),
 ("Ice","IceSpikes","AREA","IceShards","ice-spear","Spells/Ice/NE_Spie","Erupt spikes around the aimed point."),
 ("Ice","IceWall","WALL","IceSpikes","ice-golem","Spells/Ice/NE_IceWall_System","Create a temporary blocking wall at the aimed point."),
 ("Ice","IceStorm","AREA","IceWall","ice-iris","Spells/Ice/NS_IceShards","Burst ice over a broad area at the aimed point."),
 ("Electric","ElectricBolt","PROJECTILE",None,"Energy_Ball","Spells/ElectricBolt","Launch an electric projectile."),
 ("Electric","ChainLightning","CHAIN","ElectricBolt","chain-lightning","Spells/Electric/ShockBeam","Strike up to three visible enemies in range."),
 ("Electric","ElectricBlink","BLINK","ChainLightning","fire-dash","Spells/ElectricBolt","Blink forward, stopping before solid obstacles."),
 ("Earth","Root","ROOT",None,"root-tip","Casting","Immobilize and damage enemies in the forward targeting cone."),
 ("Earth","EarthWall","WALL","Root","earth-crack","Casting","Raise a temporary blocking wall at the aimed point."),
 ("Earth","EarthQuake","AREA","EarthWall","EarthQuake","Explode/Water_Burst","Damage enemies around the aimed point with a ground burst."),
 ("Water","WaterBolt","PROJECTILE",None,"Water_ball","Spells/Water/WaterBolt","Launch a water projectile."),
 ("Water","WaterHeal","HEAL","WaterBolt","Heal_Ball","Casting_Effect","Restore your health through a Gameplay Effect."),
 ("Water","WaterStream","CONE","WaterHeal","Water_Breath","Spells/Water/WaterBolt","Damage enemies in a forward cone of water."),
 ("Light","LightBolt","PROJECTILE",None,"Holy_Ball","EnergyBall","Launch a light projectile."),
 ("Light","LightHeal","HEAL","LightBolt","Heal_Ball","Casting_Effect","Restore your health through a Gameplay Effect."),
 ("Light","HolyShield","SHIELD","LightHeal","HolyShield","Casting_Effect","Gain a timed armor bonus through GAS."),
 ("Light","HolySmite","AREA","HolyShield","Smite","EnergyBall","Smite enemies around the aimed point."),
 ("Shadow","ShadowBolt","PROJECTILE",None,"Shadow_Ball","Spells/Shadow/Shadow_Ball","Launch a shadow projectile."),
 ("Shadow","ShadowShipon","DRAIN","ShadowBolt","Shadow_Breath","Spells/Shadow/Shadow_Ball","Damage enemies in front and restore half of that damage as health."),
 ("Shadow","ShadowFear","FEAR","ShadowShipon","Shadow_Ghost","Explode/Shadow_Burst","Repel visible enemies in the forward cone."),
 ("Air","AirBolt","PROJECTILE",None,"AirBolt","Spells/Rasen","Launch an air projectile."),
 ("Air","PushBack","CONE","AirBolt","mighty-force","Spells/Rasen","Damage and push enemies away in a forward cone."),
 ("Melee","Slash","MELEE",None,"saber-slash",None,"Swing your weapon; only collisions during the montage window deal damage."),
 ("Melee","Swing","MELEE","Slash","spinning-sword",None,"Sweep your weapon through an authored melee collision window."),
 ("Melee","FullSwing","MELEE","Swing","spinning-blades",None,"Perform a full weapon swing through the authored collision window."),
 ("Passive","HaloOfProtection","PASSIVE",None,"HolyShield",None,"Automatically gain 10 armor per learned rank."),
 ("Passive","LifeSiphon","PASSIVE",None,"Shadow_Breath",None,"Automatically restore 3% of damage dealt as health per rank."),
 ("Passive","ManaSiphon","PASSIVE",None,"Energy_Ball",None,"Automatically restore 2% of damage dealt as mana per rank."),
]

def backup(path):
    file = root / ("Content/" + path[len("/Game/"):].split(".")[0] + ".uasset")
    if file.exists():
        target = root / "Saved/Backups/CompleteSkillTree" / file.relative_to(root)
        if not target.exists():
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(file, target)

def save(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
        raise RuntimeError("Save failed: " + asset.get_path_name())

def asset(path):
    return unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None

# Some serialized effect paths are NiagaraEmitter assets, which ReleaseFX
# (UNiagaraSystem) cannot reference; fall back to a same-school system.
FX_FALLBACK = {"Spells/Ice/NE_Spie": "Spells/Ice/NS_IceShards", "Casting": "Casting_Effect"}
def fx(effect):
    candidate = asset(FX + effect) if effect else None
    if isinstance(candidate, unreal.NiagaraSystem): return candidate
    replacement = asset(FX + FX_FALLBACK[effect]) if effect in FX_FALLBACK else None
    return replacement if isinstance(replacement, unreal.NiagaraSystem) else None

def projectile(branch, effect):
    name = "BP_Projectile_" + branch + "Bolt"
    if branch == "Fire": name = "BP_Projectile_FireBolt"
    if branch == "Ice": name = "BP_Projectile_IceShards"
    path = "/Game/Blueprints/Gameplay/Projectiles/" + name
    bp = asset(path)
    backup(path)
    if not bp:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.RPGProjectile)
        bp = tools.create_asset(name, path.rsplit("/",1)[0], unreal.Blueprint, factory)
    cdo = unreal.get_default_object(bp.generated_class())
    effect_fx = fx(effect)
    cdo.set_editor_property("trail_fx", effect_fx)
    cdo.set_editor_property("impact_fx", asset("/Game/Art/Niagara/Explode/Fire_Burst") if branch == "Fire" else effect_fx)
    cdo.set_editor_property("impact_sound", asset("/Game/Audio/Meta/MetaSounds/SFX_Projectile"))
    if not unreal.DTStructRepairLibrary.force_compile_and_save(path): raise RuntimeError("Projectile compile failed: " + path)
    return bp.generated_class()

try:
    definitions = []
    tier_by_id = {}
    for branch, leaf, payload, prerequisite, icon, effect, description in specs:
        ability_id = "Abilities." + branch + "." + leaf
        prerequisite_id = "Abilities." + branch + "." + prerequisite if prerequisite else None
        tier = tier_by_id.get(prerequisite_id, 0) + 1
        tier_by_id[ability_id] = tier
        existing_name = {"Slash":"BasicSlash"}.get(leaf, leaf)
        path = "/Game/Data/Abilities/Player/DA_Player_" + existing_name
        definition = asset(path)
        created = not bool(definition)
        backup(path)
        if not definition:
            factory = unreal.DataAssetFactory()
            factory.set_editor_property("data_asset_class", unreal.RPGCombatAbilityDefinition)
            definition = tools.create_asset(path.rsplit("/",1)[1], path.rsplit("/",1)[0], unreal.RPGCombatAbilityDefinition, factory)
        definition.set_editor_property("ability_id", ability_id)
        definition.set_editor_property("display_name", unreal.Text(re.sub(r"(?<=[a-z])(?=[A-Z])", " ", leaf)))
        definition.set_editor_property("description", unreal.Text(description))
        definition.set_editor_property("requires_target", False)
        definition.set_editor_property("prerequisite_abilities", [unreal.Name(prerequisite_id)] if prerequisite_id else [])
        # Existing starter progression remains accessible; follow-up skills require their lower node.
        definition.set_editor_property("required_player_level", 1 if not created else tier)
        definition.set_editor_property("kind", unreal.RPGAbilityKind.PASSIVE if payload == "PASSIVE" else
                                       unreal.RPGAbilityKind.MELEE if payload == "MELEE" else unreal.RPGAbilityKind.SPELL)
        if created:
            definition.set_editor_property("maximum_rank", 3)
            definition.set_editor_property("base_damage_by_rank", [15.0*tier, 22.0*tier, 30.0*tier] if payload not in ("HEAL","SHIELD","BLINK","WALL","PASSIVE") else [0.0,0.0,0.0])
            definition.set_editor_property("attack_power_scale", 1.0 if payload == "MELEE" else 0.0)
            definition.set_editor_property("ability_power_scale", 0.0 if payload in ("MELEE","PASSIVE") else 0.6)
            definition.set_editor_property("mana_cost", 0.0 if payload in ("MELEE","PASSIVE") else 5.0 + tier*2)
            definition.set_editor_property("stamina_cost", 5.0 if payload == "MELEE" else 0.0)
            definition.set_editor_property("cooldown", 1.0 if payload == "MELEE" else 3.0 + tier)
            definition.set_editor_property("range", 220.0 if payload == "MELEE" else 600.0 if payload == "BLINK" else 1600.0)
        definition.set_editor_property("effect_radius", 550.0 if leaf in ("MeteorStrike","IceStorm","EarthQuake") else 300.0)
        definition.set_editor_property("effect_duration", 6.0 if payload in ("SHIELD","WALL") else 2.0)
        definition.set_editor_property("effect_magnitude", 650.0 if leaf == "PushBack" else 25.0)
        definition.set_editor_property("execution", unreal.RPGSkillExecution.DAMAGE if payload in ("MELEE","PROJECTILE","PASSIVE") else getattr(unreal.RPGSkillExecution, payload))
        definition.set_editor_property("release_fx", fx(effect) if effect and payload != "PROJECTILE" else None)
        if payload == "PROJECTILE": definition.set_editor_property("projectile_class", projectile(branch, effect))
        elif created: definition.set_editor_property("projectile_class", None)
        if payload != "PASSIVE" and payload != "MELEE":
            if not unreal.DTStructRepairLibrary.configure_ability_release(path, "Event.Animation.SpellRelease",
                    definition.get_editor_property("projectile_class").get_path_name() if payload == "PROJECTILE" else "", "hand_r"):
                # Newly-created data assets must first be saved before a path-based native helper can load them.
                save(definition)
                if not unreal.DTStructRepairLibrary.configure_ability_release(path, "Event.Animation.SpellRelease",
                        definition.get_editor_property("projectile_class").get_path_name() if payload == "PROJECTILE" else "", "hand_r"):
                    raise RuntimeError("Release setup failed: " + ability_id)
        presentation = definition.get_editor_property("presentation")
        image = asset("/Game/UI/2D/Skills/SKill_Icons/" + icon)
        if image: presentation.set_editor_property("icon", image)
        background = asset("/Game/UI/2D/GUI/ICons/M_SkillBackGround")
        if background: presentation.set_editor_property("background_material", background)
        definition.set_editor_property("presentation", presentation)
        save(definition)
        definitions.append(definition)
        report["skills"].append({"id": ability_id, "payload": payload, "prerequisites": [prerequisite_id] if prerequisite_id else [], "created": created})

    table_path = "/Game/Data/Catalogues/DT_PlayerAbilities"
    catalogue = asset(table_path)
    backup(table_path)
    if not catalogue:
        factory = unreal.DataTableFactory()
        factory.set_editor_property("struct", unreal.load_object(None,"/Script/DuTraveller.RPGAbilityCatalogueRow"))
        catalogue = tools.create_asset("DT_PlayerAbilities", "/Game/Data/Catalogues", unreal.DataTable, factory)
    # Keep any additional user-authored catalogue rows.
    rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(catalogue) or "[]")
    by_name = {row["Name"]:row for row in rows}
    by_name.update({str(d.get_editor_property("ability_id")): {"Name":str(d.get_editor_property("ability_id")), "Definition":d.get_path_name()} for d in definitions})
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(catalogue,json.dumps(list(by_name.values()))):
        raise RuntimeError("Catalogue import failed")
    save(catalogue)
    state_path = "/Game/Blueprints/Framework/BP_PlayerState"
    backup(state_path)
    state_bp = asset(state_path)
    state = unreal.get_default_object(state_bp.generated_class())
    skills = state.get_editor_property("skills")
    skills.set_editor_property("ability_catalogue", catalogue)
    skills.set_editor_property("hotbar_size", 12)
    if not unreal.DTStructRepairLibrary.force_compile_and_save(state_path): raise RuntimeError("PlayerState compile failed")
    player_path = "/Game/Blueprints/Characters/BP_PlayerCharacter"
    backup(player_path)
    player_bp = asset(player_path)
    router = unreal.get_default_object(player_bp.generated_class()).get_editor_property("animation_router")
    mapping = dict(router.get_editor_property("combat_animations"))
    for definition in definitions:
        if definition.get_editor_property("kind") == unreal.RPGAbilityKind.PASSIVE: continue
        key = definition.get_editor_property("ability_id")
        if key not in mapping:
            entry = unreal.DTCombatAnimation()
            entry.set_editor_property("montage", asset(melee_path if definition.get_editor_property("kind") == unreal.RPGAbilityKind.MELEE else cast_path))
            mapping[key] = entry
    router.set_editor_property("combat_animations", mapping)
    if not unreal.DTStructRepairLibrary.force_compile_and_save(player_path): raise RuntimeError("Player compile failed")
    for name in ("WBP_SpellMenu","WBP_MageSpellTree","WBP_WarriorSpellTree","WBP_MageTalentTree","WBP_EquippedSpell_Row"):
        path = "/Game/UI/SpellMenu/" + name
        backup(path)
        report["widgets"][path] = list(unreal.DTStructRepairLibrary.inspect_skill_widgets(path, True))
        unreal.DTStructRepairLibrary.dump_blueprint_graph(path)
    for path in ("/Game/UI/HUD/SpellGlobes/SpellBoard/WBP_EquippedRow_Button", "/Game/UI/HUD/SpellGlobes/SpellBoard/WBP_SpellGlobe_Button"):
        report["widgets"][path] = list(unreal.DTStructRepairLibrary.inspect_skill_widgets(path, False))
        unreal.DTStructRepairLibrary.dump_blueprint_graph(path)
    evidence = json.loads((root/"Docs/ClassSkillTreeEvidence.json").read_text(encoding="utf-8"))
    missing = {tag for row in evidence for tag in row["SerializedAbilityTags"]} - {str(d.get_editor_property("ability_id")) for d in definitions}
    if missing: raise RuntimeError("Tree skills still missing: " + str(missing))
    report["passed"] = True
except Exception:
    import traceback
    report["errors"].append(traceback.format_exc())
    unreal.log_error(report["errors"][-1])
finally:
    (root/"Saved/Automation/CompleteSkillTree.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
    unreal.log("COMPLETE_SKILL_TREE " + json.dumps(report))
