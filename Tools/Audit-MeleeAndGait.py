import json
import shutil
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir())
report={"weapons":[],"player":{}}
bp=unreal.load_asset("/Game/Blueprints/Characters/BP_PlayerCharacter")
cdo=unreal.get_default_object(bp.generated_class())
for name in ("AnalogWalk/RunThreshold","SprintSpeeds","RunSpeeds","WalkSpeeds","Gait","StartingEquipment"):
    try: report["player"][name]=str(cdo.get_editor_property(name))
    except Exception as exc: report["player"][name]=str(exc)
subsystem=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
for itemclass in cdo.get_editor_property("starting_equipment"):
    item=unreal.get_default_object(itemclass)
    mesh=item.get_editor_property("equipped_mesh")
    if not mesh: continue
    count=subsystem.get_simple_collision_count(mesh) if subsystem else -1
    row={"item":itemclass.get_path_name(),"mesh":mesh.get_path_name(),"simple_collisions":count,"socket":str(item.get_editor_property("attach_socket")),"transform":str(item.get_editor_property("attachment_transform"))}
    if count==0:
        path=mesh.get_path_name().split(".")[0]
        src=root/("Content/"+path[len("/Game/"):]+".uasset")
        backup=root/"Saved/Backups/WeaponCollision"/src.relative_to(root)
        backup.parent.mkdir(parents=True,exist_ok=True)
        if not backup.exists(): shutil.copy2(src,backup)
        result=subsystem.add_simple_collisions(mesh,unreal.ScriptCollisionShapeType.BOX)
        if result<0 or not unreal.EditorAssetLibrary.save_loaded_asset(mesh,False): raise RuntimeError("Weapon collision repair failed")
        row["repaired_simple_collisions"]=subsystem.get_simple_collision_count(mesh)
    report["weapons"].append(row)
unreal.DTStructRepairLibrary.dump_blueprint_graph("/Game/Blueprints/Characters/BP_PlayerCharacter")
(root/"Saved/Automation/MeleeAndGaitAudit.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
unreal.log("MELEE_GAIT_AUDIT "+json.dumps(report))
