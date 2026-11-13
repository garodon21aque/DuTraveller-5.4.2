"""Author an explicit editable starter recipe, its item classes and consumed table.

These are new defaults, not claimed as recovered original crafting balance.
"""
import json
from pathlib import Path
import traceback
import unreal

report = {"passed": False, "provenance": "New editable starter crafting content", "assets": []}
tools = unreal.AssetToolsHelpers.get_asset_tools()

def blueprint(path, parent, properties):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        asset = unreal.load_asset(path)
        report["assets"].append({"asset": path, "preserved": True})
        return asset.generated_class()
    folder, name = path.rsplit("/", 1)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    asset = tools.create_asset(name, folder, unreal.Blueprint, factory)
    default = unreal.get_default_object(asset.generated_class())
    for prop, value in properties.items():
        default.set_editor_property(prop, value)
    if not unreal.DTStructRepairLibrary.force_compile_and_save(path):
        raise RuntimeError("Blueprint compile/save failed: " + path)
    report["assets"].append({"asset": path, "preserved": False})
    return asset.generated_class()

try:
    herb = blueprint("/Game/Data/Items/BP_DT_WildHerb", unreal.ItemObject,
                     dict(item_display_name=unreal.Text("Wild herb"), item_description=unreal.Text("An ingredient used in basic alchemy."),
                          stackable=True, max_stack_size=50, price=1,
                          thumbnail=unreal.load_asset("/Game/UI/2D/Skills/flamed-leaf")))
    potion = blueprint("/Game/Data/Items/BP_DT_HealthPotion", unreal.Potion_ItemObject,
                       dict(item_display_name=unreal.Text("Health potion"), item_description=unreal.Text("Restores 25 health."),
                            use_action_text=unreal.Text("Drink"), stackable=True, max_stack_size=20, price=5,
                            thumbnail=unreal.load_asset("/Game/UI/2D/Items/Potions"),
                            use_effect=unreal.RPGConsumableResourceEffect, set_by_caller_magnitudes={"Health": 25.0}))
    bronze = unreal.load_class(None, "/Game/Blueprints/InventorySystem/Items/MoneyItem/BP_BronzeItem.BP_BronzeItem_C")
    if not bronze:
        raise RuntimeError("The existing bronze currency class is missing")
    ingredients = []
    for cls, quantity in ((herb, 2), (bronze, 1)):
        entry = unreal.RPGInventoryAmount()
        entry.set_editor_property("item_class", cls)
        entry.set_editor_property("quantity", quantity)
        ingredients.append(entry)
    recipe = blueprint("/Game/Data/Recipes/BP_DT_HealthPotionRecipe", unreal.ItemBlueprint,
                       dict(item_display_name=unreal.Text("Brew health potion"),
                            item_description=unreal.Text("Brew a health potion using two wild herbs and one bronze coin."),
                            thumbnail=unreal.load_asset("/Game/UI/2D/Items/Potions"),
                            ingredients=ingredients, required_items=[herb, herb, bronze], crafted_item=potion, crafted_quantity=1))
    path = "/Game/Data/Catalogues/DT_CraftingRecipes"
    table = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    struct = unreal.load_object(None, "/Script/DuTraveller.RPGRecipeCatalogueRow")
    if not table:
        factory = unreal.DataTableFactory(); factory.set_editor_property("struct", struct)
        table = tools.create_asset("DT_CraftingRecipes", "/Game/Data/Catalogues", unreal.DataTable, factory)
    if unreal.DataTableFunctionLibrary.get_data_table_row_struct(table) != struct:
        raise RuntimeError("Crafting table has an incompatible row schema")
    if not unreal.DataTableFunctionLibrary.get_data_table_row_names(table):
        rows = [{"Name": "Recipe.HealthPotion", "RecipeClass": recipe.get_path_name()}]
        if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows), struct):
            raise RuntimeError("Crafting table import failed")
        if not unreal.EditorAssetLibrary.save_loaded_asset(table, False):
            raise RuntimeError("Crafting table save failed")
    character_path = "/Game/Blueprints/Characters/BP_PlayerCharacter"
    character_bp = unreal.load_asset(character_path)
    character = unreal.get_default_object(character_bp.generated_class())
    character.get_editor_property("crafting_comp").set_editor_property("recipe_catalogue", table)
    if not unreal.DTStructRepairLibrary.force_compile_and_save(character_path):
        raise RuntimeError("Active character Blueprint compile/save refused the crafting table")
    report["table"] = table.get_path_name()
    report["rows"] = [str(name) for name in unreal.DataTableFunctionLibrary.get_data_table_row_names(table)]
    report["passed"] = True
except Exception:
    report["exception"] = traceback.format_exc()
    raise
finally:
    output = Path(unreal.Paths.project_saved_dir()) / "Automation/StarterCraftingData.json"
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("DT_STARTER_CRAFTING " + json.dumps(report))
    unreal.SystemLibrary.quit_editor()
