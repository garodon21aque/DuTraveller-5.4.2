import json
from pathlib import Path
import unreal

report={"widgets":{},"animation":{},"pose_api":{}}
for path in ("/Game/UI/World/WBP_DeathWidget","/Game/UI/World/WBP_EnemyHealthWidget","/Game/UI/World/WPB_TargetEnemy"):
    asset=unreal.load_asset(path)
    if not asset: continue
    unreal.DTStructRepairLibrary.dump_blueprint_graph(path)
    unreal.BlueprintEditorLibrary.compile_blueprint(asset)
    report["widgets"][path]={"parent":str(unreal.BlueprintEditorLibrary.get_blueprint_parent_class(asset)),"status":str(asset.get_editor_property("status"))}
for path in unreal.EditorAssetLibrary.list_assets("/Game/Art/3D/Player/Animations/Anims",True,False):
    if "LocomotionBS" not in path: continue
    asset=unreal.load_asset(path)
    row={"type":asset.get_class().get_name()}
    for name in ("skeleton","sample_data","blend_parameters"):
        try: row[name]=str(asset.get_editor_property(name))
        except Exception as exc: row[name]=str(exc)
    report["animation"][path]=row
for name in ("AnimPoseExtensions","AnimPoseEvaluationOptions","AnimPoseSpaces","AnimationLibrary"):
    if hasattr(unreal,name): report["pose_api"][name]=[x for x in dir(getattr(unreal,name)) if any(v in x.lower() for v in ("pose","bone","root","space"))]
report["status_assets"]=[p for p in unreal.EditorAssetLibrary.list_assets("/Game/UI",True,False) if any(v in p.lower() for v in ("stun","burn","spott","shock","debuff","status","slow","root","alert"))]
Path(unreal.Paths.project_saved_dir(),"Automation/DeathEnemyPresentationAudit.json").write_text(json.dumps(report,indent=2))
unreal.log("DEATH_ENEMY_AUDIT "+json.dumps(report))
