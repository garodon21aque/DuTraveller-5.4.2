"""Apply the backed-up tooltip graph repair, then run the real PIE suite."""
from pathlib import Path
import unreal

tooltip = "/Game/UI/CraftingSystem/WBP_ItemBpTooltip"
if not unreal.DTStructRepairLibrary.repair_unified_widget_context(tooltip):
    raise RuntimeError("Recipe tooltip repair failed")
if not unreal.DTStructRepairLibrary.force_compile_and_save(tooltip):
    raise RuntimeError("Recipe tooltip failed compile/save guard")
unreal.log("DT_CRAFTING_TOOLTIP_REPAIRED " + tooltip)
test = Path(unreal.Paths.project_dir()) / "Tools/Test-UnifiedPlayerUI.py"
exec(compile(test.read_text(encoding="utf-8"), str(test), "exec"), globals())
