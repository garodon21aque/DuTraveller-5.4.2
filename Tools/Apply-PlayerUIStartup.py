"""Repair backed-up player UI context and author explicit, editable starter skill data."""
import json
from pathlib import Path
import unreal

report = {"repaired": [], "starter_abilities": [], "failed_assets": []}
repair = unreal.DTStructRepairLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

def save_blueprint(path):
    if not repair.force_compile_and_save(path):
        raise RuntimeError("Compile/save guard refused " + path)
    report["repaired"].append(path)

try:
    inventory = "/Game/UI/InventorySystem/WBP_PlayerInventory"
    bp = unreal.load_asset(inventory)
    unreal.BlueprintEditorLibrary.reparent_blueprint(bp, unreal.DTPlayerInventoryWidget)
    quest_bp = unreal.load_asset("/Game/UI/QuestSystem/WBP_QuestLog")
    unreal.BlueprintEditorLibrary.reparent_blueprint(quest_bp, unreal.DTQuestJournalWidget)
    drop_bp = unreal.load_asset("/Game/UI/InventorySystem/WBP_DropWidget")
    unreal.BlueprintEditorLibrary.reparent_blueprint(drop_bp, unreal.DTDropQuantityWidget)
    for path in (inventory, "/Game/UI/HUD/WBP_HUD", "/Game/UI/CraftingSystem/WBP_CraftingMenu",
                 "/Game/UI/QuestSystem/WBP_QuestLog", "/Game/UI/InventorySystem/WBP_DropWidget",
                 "/Game/UI/InventorySystem/WBP_PlayerStat"):
        repair.repair_unified_widget_context(path)
        if path.endswith("WBP_HUD"):
            repair.replace_blueprint_call_function(path, "AddToViewport", "/Script/DuTraveller.DTUIRoot", "AttachPlayerNotification")
            repair.replace_blueprint_call_function(path, "OpenCloseAttributes", "/Script/DuTraveller.RPG_AbilitySystemLibrary", "ToggleAttributesPanel")
            repair.replace_blueprint_call_function(path, "CloseAttributes", "/Script/DuTraveller.RPG_AbilitySystemLibrary", "ClosePlayerPanels")
            repair.refresh_blueprint_nodes(path)
        if path.endswith("WBP_QuestLog"):
            repair.replace_blueprint_call_function(path, "RemoveFromParent", "/Script/DuTraveller.RPG_AbilitySystemLibrary", "ClosePlayerPanels")
            repair.refresh_blueprint_nodes(path)
        try:
            save_blueprint(path)
        except RuntimeError as error:
            report["failed_assets"].append({"asset": path, "error": str(error)})

    # These starter values are authored here, not claimed as recovered historical balance.
    specs = [
        ("DA_Player_BasicSlash", "Abilities.Melee.Slash", "Slash", unreal.RPGAbilityKind.MELEE, 14.0, 0.0, 0.0, 225.0, 0.65),
        ("DA_Player_FireBolt", "Abilities.Fire.FireBolt", "Fire Bolt", unreal.RPGAbilityKind.SPELL, 22.0, 8.0, 0.0, 1200.0, 1.25),
        ("DA_Player_FullSwing", "Abilities.Melee.FullSwing", "Full Swing", unreal.RPGAbilityKind.MELEE, 26.0, 0.0, 10.0, 250.0, 1.5),
        ("DA_Player_IceShards", "Abilities.Ice.IceShards", "Ice Shards", unreal.RPGAbilityKind.SPELL, 18.0, 6.0, 0.0, 1000.0, 1.0),
    ]
    definitions = []
    icons = ["/Game/UI/2D/Skills/SKill_Icons/saber-slash", "/Game/UI/2D/Skills/Fireball_Icon",
             "/Game/UI/2D/Skills/SKill_Icons/spinning-sword", "/Game/UI/2D/Skills/SKill_Icons/ice-spear"]
    for name, ability_id, title, kind, damage, mana, stamina, distance, cooldown in specs:
        path = "/Game/Data/Abilities/Player/" + name
        asset = unreal.load_asset(path)
        if not asset:
            factory = unreal.DataAssetFactory()
            factory.set_editor_property("data_asset_class", unreal.RPGCombatAbilityDefinition)
            asset = tools.create_asset(name, "/Game/Data/Abilities/Player", unreal.RPGCombatAbilityDefinition, factory)
            values = dict(ability_id=ability_id, display_name=unreal.Text(title), kind=kind,
                          description=unreal.Text(title + ": starter combat ability. Balance is editable in this Data Asset."),
                          maximum_rank=3, required_player_level=1, base_damage_by_rank=[damage, damage * 1.3, damage * 1.6],
                          attack_power_scale=1.0 if kind == unreal.RPGAbilityKind.MELEE else 0.0,
                          ability_power_scale=1.0 if kind == unreal.RPGAbilityKind.SPELL else 0.0,
                          mana_cost=mana, stamina_cost=stamina, range=distance, cooldown=cooldown)
            for prop, value in values.items():
                asset.set_editor_property(prop, value)
            unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        presentation = asset.get_editor_property("presentation")
        if not presentation.get_editor_property("icon"):
            presentation.set_editor_property("icon", unreal.load_asset(icons[len(definitions)]))
            if not presentation.get_editor_property("background_material"):
                presentation.set_editor_property("background_material", unreal.load_asset("/Game/UI/2D/GUI/ICons/MI_Globe"))
            asset.set_editor_property("presentation", presentation)
            unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        definitions.append(asset)
        report["starter_abilities"].append(asset.get_path_name())

    path = "/Game/Blueprints/Framework/BP_PlayerState"
    state_bp = unreal.load_asset(path)
    state = unreal.get_default_object(state_bp.generated_class())
    state.set_editor_property("starting_attribute_points", 5)
    state.set_editor_property("starting_skill_points", 3)
    skills = state.get_editor_property("skills")
    available = [entry for entry in skills.get_editor_property("available_abilities")
                 if entry and entry.get_path_name() != "/Game/Data/Abilities/DA_AI_BasicAttack.DA_AI_BasicAttack"]
    for definition in definitions:
        if definition not in available:
            available.append(definition)
    skills.set_editor_property("available_abilities", available)
    if not skills.get_editor_property("starter_hotbar"):
        hotbar = []
        for index, definition in enumerate(definitions[:2]):
            entry = unreal.RPGHotbarSlot()
            entry.set_editor_property("slot", index)
            entry.set_editor_property("definition", definition)
            hotbar.append(entry)
        skills.set_editor_property("starter_hotbar", hotbar)
    save_blueprint(path)
    for path in ("/Game/Blueprints/Framework/DT_GameModeBase", "/Game/Blueprints/Framework/BP_RPGGameModeBase"):
        mode_bp = unreal.load_asset(path)
        mode = unreal.get_default_object(mode_bp.generated_class())
        for prop, class_path in (
            ("default_pawn_class", "/Game/Blueprints/Characters/BP_PlayerCharacter.BP_PlayerCharacter_C"),
            ("player_controller_class", "/Game/Blueprints/Framework/BP_PlayerController.BP_PlayerController_C"),
            ("player_state_class", "/Game/Blueprints/Framework/BP_PlayerState.BP_PlayerState_C"),
            ("hud_class", "/Script/DT.DT_HUD"),
        ):
            mode.set_editor_property(prop, unreal.load_class(None, class_path))
        save_blueprint(path)
    report["passed"] = not report["failed_assets"]
finally:
    output = Path(unreal.Paths.project_saved_dir()) / "Automation" / "PlayerUIStartup.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("PLAYER_UI_STARTUP " + json.dumps(report))
    unreal.SystemLibrary.quit_editor()
