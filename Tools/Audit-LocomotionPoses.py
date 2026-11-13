import json
from pathlib import Path
import unreal
report=[]
blend=unreal.load_asset("/Game/Art/3D/Player/Animations/Anims/LocomotionBS")
assets=[s.get_editor_property("animation") for s in blend.get_editor_property("sample_data")]
for path in ("/Game/Art/3D/Player/Animations/Anims/Retargeted/Standing_1H_Cast_Spell_01_mixamo_com","/Game/Art/3D/Player/Animations/Anims/Retargeted/RootMixamoUE4/Walking","/Game/Art/3D/Player/Animations/Anims/Retargeted/RootMixamoUE4/Unarmed_Idle","/Game/Art/3D/Player/Animations/Anims/Retargeted/RootMixamoUE4/Run"):
    a=unreal.load_asset(path)
    if a: assets.append(a)
for asset in assets:
    row={"asset":asset.get_path_name(),"skeleton":str(asset.get_editor_property("skeleton")),"bones":{}}
    try:
        pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(asset,0.2,unreal.AnimPoseEvaluationOptions())
        for bone in ("root","pelvis","head","foot_l","foot_r"):
            transform=unreal.AnimPoseExtensions.get_bone_pose(pose,bone,unreal.AnimPoseSpaces.WORLD)
            row["bones"][bone]=str(transform)
    except Exception as exc: row["error"]=str(exc)
    report.append(row)
Path(unreal.Paths.project_saved_dir(),"Automation/LocomotionPoses.json").write_text(json.dumps(report,indent=2))
