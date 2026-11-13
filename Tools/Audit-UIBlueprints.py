"""Compile selected UI/controller Blueprints in memory and report status; never save packages."""
import json
from pathlib import Path
import unreal

ASSETS = [
    "/Game/Blueprints/Controllers/BP_RPGAttributeWidgetController",
    "/Game/Blueprints/Controllers/BP_RPGOverlayWidgetController",
    "/Game/Blueprints/Controllers/BP_RPGSpellWidgetController",
    "/Game/UI/HUD/WBP_HUD",
    "/Game/UI/Attribute/WBP_AttributeMenu",
    "/Game/UI/Attribute/WBP_AttributesPointsRow",
    "/Game/UI/Attribute/WBP_TextValueButtonROw",
    "/Game/UI/Attribute/WBP_TextValueRow",
    "/Game/UI/SpellMenu/WBP_SpellMenu",
    "/Game/UI/HUD/SpellGlobes/WBP_SpellGlobe",
    "/Game/UI/HUD/SpellGlobes/SpellBoard/WBP_SpellGlobe_Button",
    "/Game/UI/HUD/SpellGlobes/SpellBoard/WBP_EquippedRow_Button",
    "/Game/UI/HUD/SpellGlobes/AttributeGlobes/WBP_HealthGlobe",
    "/Game/UI/HUD/SpellGlobes/AttributeGlobes/WBP_ManaGlobe",
    "/Game/UI/HUD/SpellGlobes/AttributeGlobes/WBP_LevelGlobe",
    "/Game/UI/InventorySystem/WBP_PlayerInventory",
    "/Game/UI/InventorySystem/WBP_DropWidget",
    "/Game/UI/InventorySystem/WBP_ItemObject",
    "/Game/UI/InventorySystem/WBP_InvItemTooltip",
    "/Game/UI/InventorySystem/WBP_EquipmentToolBar",
    "/Game/UI/InventorySystem/WBP_VendorItemObject",
    "/Game/UI/CraftingSystem/WBP_ItemBP",
    "/Game/UI/CraftingSystem/WBP_ItemBpTooltip",
    "/Game/UI/QuestSystem/WBP_QuestLog",
    "/Game/Blueprints/InventorySystem/Items/MoneyItem/BP_BronzeItem",
    "/Game/Blueprints/InventorySystem/Items/MoneyItem/BP_GoldItem",
    "/Game/Blueprints/Actors/Interactables/Items/BP_MasterPickupItem",
    "/Game/Blueprints/Actors/Interactables/Items/BP_Pickup",
]

rows = []
try:
    for path in ASSETS:
        asset = unreal.load_asset(path)
        if not asset:
            rows.append({"asset": path, "loaded": False, "status": "Missing"})
            continue
        try:
            unreal.BlueprintEditorLibrary.compile_blueprint(asset)
            status = str(asset.get_editor_property("status"))
        except Exception as exc:
            status = "CompileException: " + str(exc)
        parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(asset)
        rows.append({"asset": path, "loaded": True, "status": status,
                     "parent": parent.get_path_name() if parent else None})
    report = {"saved_packages": False, "assets": rows}
    output = Path(unreal.Paths.project_saved_dir()) / "Automation" / "UIBlueprintAudit.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("UI_BLUEPRINT_AUDIT " + json.dumps(report))
finally:
    unreal.SystemLibrary.quit_editor()
