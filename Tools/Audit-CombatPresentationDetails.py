import json
from pathlib import Path
import unreal
report={"widgets":{},"samples":[],"api":{},"graphs":{}}
for path in ("/Game/UI/World/WBP_EnemyHealthWidget","/Game/UI/World/WBP_DeathWidget","/Game/UI/World/WPB_TargetEnemy"):
    asset=unreal.load_asset(path)
    row=[]
    try:
        tree=asset.get_editor_property("widget_tree")
        def visit(widget):
            item={"name":widget.get_name(),"class":widget.get_class().get_name()}
            if isinstance(widget,unreal.Image): item["resource"]=str(widget.get_editor_property("brush").resource_object)
            if isinstance(widget,unreal.TextBlock): item["text"]=str(widget.get_text())
            row.append(item)
            if isinstance(widget,unreal.PanelWidget):
                for child in widget.get_all_children(): visit(child)
        visit(tree.get_editor_property("root_widget"))
    except Exception as exc: row.append({"error":str(exc)})
    report["widgets"][path]=row
for path in ("/Game/Blueprints/Animations/ABP_SandboxCharacter","/Game/Blueprints/Gameplay/Projectiles/BP_Projectile_FireBolt"):
    unreal.DTStructRepairLibrary.dump_blueprint_graph(path)
    report["graphs"][path]=list(unreal.DTStructRepairLibrary.describe_anim_graph(path)) if "Sandbox" in path else []
for name in ("get_anim_pose_at_time","get_bone_pose"):
    report["api"][name]=getattr(unreal.AnimPoseExtensions,name).__doc__
blend=unreal.load_asset("/Game/Art/3D/Player/Animations/Anims/LocomotionBS")
for sample in blend.get_editor_property("sample_data"):
    row={}
    for name in ("animation","sample_value"):
        try: row[name]=str(sample.get_editor_property(name))
        except Exception as exc: row[name]=str(exc)
    report["samples"].append(row)
Path(unreal.Paths.project_saved_dir(),"Automation/CombatPresentationDetails.json").write_text(json.dumps(report,indent=2))
