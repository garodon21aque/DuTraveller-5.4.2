"""Repair the backed-up recipe tooltip's rarity source and save only on compile success."""
import unreal
path = "/Game/UI/CraftingSystem/WBP_ItemBpTooltip"
if not unreal.DTStructRepairLibrary.repair_unified_widget_context(path):
    raise RuntimeError("Recipe tooltip context repair failed")
if not unreal.DTStructRepairLibrary.force_compile_and_save(path):
    raise RuntimeError("Recipe tooltip compile/save guard refused the repair")
unreal.log("DT_CRAFTING_TOOLTIP_REPAIRED " + path)
unreal.SystemLibrary.quit_editor()
