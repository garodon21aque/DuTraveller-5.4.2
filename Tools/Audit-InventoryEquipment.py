import json
from pathlib import Path
import unreal

report={"widgets":{},"drag_operations":{}}
for name in ("WBP_PlayerInventory","WBP_Equipment","WBP_EquipmentSlot","WBP_EquipmentToolBar","WBP_ItemObject","WBP_InvItemTooltip","WBP_PlayerStat"):
    path="/Game/UI/InventorySystem/"+name
    bp=unreal.load_asset(path)
    cdo=unreal.get_default_object(bp.generated_class())
    row={"class":cdo.get_class().get_path_name(),"native_inventory_parent":isinstance(cdo,unreal.DTPlayerInventoryWidget),"native_item_parent":isinstance(cdo,unreal.RPG_ItemWidget),"layout":list(unreal.DTStructRepairLibrary.inspect_skill_widgets(path,False))}
    report["widgets"][name]=row
    unreal.DTStructRepairLibrary.dump_blueprint_graph(path)
for name in ("BP_ItemDragDrop","BP_ItemDragDropOperation","BP_EquipmentDragDrop","BP_EquipmentDragDropOperation"):
    path="/Game/UI/InventorySystem/DragDrop/"+name
    bp=unreal.load_asset(path)
    report["drag_operations"][name]={"is_drag_operation":isinstance(unreal.get_default_object(bp.generated_class()),unreal.DragDropOperation)}
    unreal.DTStructRepairLibrary.dump_blueprint_graph(path)
out=Path(unreal.Paths.project_saved_dir())/"Automation/InventoryEquipmentAudit.json"
out.write_text(json.dumps(report,indent=2),encoding="utf-8")
unreal.log("INVENTORY_EQUIPMENT_AUDIT "+str(out))
