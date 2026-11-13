"""Repair duplicate montage slot names without deleting authored animation tracks."""
import json
import shutil
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
report = {"passed":False,"montages":{},"anim_graph":[],"errors":[]}
try:
    for path in unreal.EditorAssetLibrary.list_assets("/Game/DTIntegration/Animation/Combat",True,False):
        montage = unreal.load_asset(path)
        if not isinstance(montage,unreal.AnimMontage): continue
        before = list(unreal.DTStructRepairLibrary.repair_duplicate_montage_slots(path,False))
        slots = [line.split(" slot=",1)[1].split(" ",1)[0] for line in before if line.startswith("track=")]
        row={"before":before,"changed":len(slots)!=len(set(slots))}
        if row["changed"]:
            package=path.split(".")[0]
            source=root/("Content/"+package[len("/Game/"):]+".uasset")
            backup=root/"Saved/Backups/MontageDuplicateSlots"/source.relative_to(root)
            backup.parent.mkdir(parents=True,exist_ok=True)
            if not backup.exists(): shutil.copy2(source,backup)
            row["repair"]=list(unreal.DTStructRepairLibrary.repair_duplicate_montage_slots(path,True))
            if "SAVE_FAILED" in row["repair"]: raise RuntimeError("Could not save "+path)
        row["after"]=list(unreal.DTStructRepairLibrary.repair_duplicate_montage_slots(path,False))
        finalslots=[line.split(" slot=",1)[1].split(" ",1)[0] for line in row["after"] if line.startswith("track=")]
        if len(finalslots)!=len(set(finalslots)): raise RuntimeError("Duplicate slots remain: "+path)
        report["montages"][path]=row
    report["anim_graph"]=list(unreal.DTStructRepairLibrary.describe_anim_graph("/Game/Blueprints/Animations/ABP_SandboxCharacter"))
    unreal.DTStructRepairLibrary.dump_blueprint_graph("/Game/Blueprints/Animations/ABP_SandboxCharacter")
    unreal.DTStructRepairLibrary.dump_blueprint_graph("/Game/Blueprints/Characters/BP_PlayerCharacter")
    report["passed"]=True
except Exception:
    import traceback
    report["errors"].append(traceback.format_exc())
    unreal.log_error(report["errors"][-1])
finally:
    output=root/"Saved/Automation/MontageDuplicateSlotRepair.json"
    output.write_text(json.dumps(report,indent=2),encoding="utf-8")
    unreal.log("MONTAGE_SLOT_REPAIR "+json.dumps(report))
