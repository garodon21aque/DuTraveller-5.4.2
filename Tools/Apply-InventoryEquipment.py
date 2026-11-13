"""Back up and connect existing inventory/equipment Blueprints to native handlers."""
import json
import shutil
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir())
report={"passed":False,"assets":[],"errors":[]}
targets=[
 ("/Game/UI/InventorySystem/WBP_EquipmentSlot",unreal.DTEquipmentSlotWidget),
 ("/Game/UI/InventorySystem/WBP_Equipment",unreal.DTEquipmentWidget),
 ("/Game/UI/InventorySystem/DragDrop/BP_ItemDragDropOperation",unreal.DTInventoryDragDrop),
 ("/Game/UI/InventorySystem/DragDrop/BP_EquipmentDragDropOperation",unreal.DTInventoryDragDrop),
 ("/Game/UI/InventorySystem/WBP_PlayerInventory",unreal.DTPlayerInventoryWidget),
]
try:
    for path,parent in targets:
        src=root/("Content/"+path[len("/Game/"):]+".uasset")
        backup=root/"Saved/Backups/InventoryEquipment"/src.relative_to(root)
        backup.parent.mkdir(parents=True,exist_ok=True)
        if not backup.exists(): shutil.copy2(src,backup)
        bp=unreal.load_asset(path)
        unreal.BlueprintEditorLibrary.reparent_blueprint(bp,parent)
        if not unreal.DTStructRepairLibrary.force_compile_and_save(path): raise RuntimeError("Compile failed: "+path)
        report["assets"].append(path)
    for path in ("/Game/UI/InventorySystem/WBP_ItemObject","/Game/UI/InventorySystem/WBP_InvItemTooltip"):
        if not unreal.DTStructRepairLibrary.force_compile_and_save(path): raise RuntimeError("Compile failed: "+path)
    report["passed"]=True
except Exception:
    import traceback
    report["errors"].append(traceback.format_exc())
    unreal.log_error(report["errors"][-1])
finally:
    (root/"Saved/Automation/InventoryEquipmentApply.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
    unreal.log("INVENTORY_EQUIPMENT_APPLY "+json.dumps(report))
