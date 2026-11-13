import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir()); result={'rolls':[],'state':{}}
for path in ['/Game/Blueprints/Framework/BP_PlayerState','/Game/Blueprints/Framework/BP_RPGPlayerState','/Game/UI/HUD/WBP_HUD','/Game/UI/HUD/WBP_Experience','/Game/UI/HUD/SpellGlobes/AttributeGlobes/WBP_ExperienceGlobe','/Game/UI/World/WBP_LevelUpNotification']:
    unreal.DTStructRepairLibrary.dump_blueprint_graph(path)
    if 'PlayerState' in path:
        bp=unreal.load_asset(path)
        if bp:
            cdo=unreal.get_default_object(bp.generated_class()); result['state'][path]={n:str(cdo.get_editor_property(n)) for n in ['LVL','EXP','required_xp','max_experience']}
for path in ['/Game/Art/3D/Player/Animations/Anims/Retargeted/Stand_To_Roll_inplace_mixamo_com','/Game/Art/3D/Player/Animations/Anims/Retargeted/RootMixamoUE4/Stand_To_Roll_in_Place','/Game/Art/3D/Player/Animations/Anims/Retargeted/RootMixamoUE4/Stand_To_Roll','/Game/Art/3D/Player/Animations/Anims/Retargeted/Locomotion/Stand_To_Roll_Anim','/Game/DTIntegration/Animation/Avoidance/DT_Stand_To_Roll_inplace_mixamo_com_UEFN']:
    a=unreal.load_asset(path); row={'path':path,'length':a.get_play_length(),'root_motion':a.get_editor_property('enable_root_motion'),'force_root_lock':a.get_editor_property('force_root_lock'),'poses':[]}
    for f in [0,.2,.4,.6,.8,1]:
        p=unreal.AnimPoseExtensions.get_anim_pose_at_time(a,a.get_play_length()*f,unreal.AnimPoseEvaluationOptions()); item={'fraction':f}
        for bone in ['root','pelvis','head']:
            tr=unreal.AnimPoseExtensions.get_bone_pose(p,bone,unreal.AnimPoseSpaces.WORLD); item[bone]={'translation':[tr.translation.x,tr.translation.y,tr.translation.z],'rotation':str(tr.rotation)}
        row['poses'].append(item)
    result['rolls'].append(row)
(root/'Saved/Automation/XPRollAudit.json').write_text(json.dumps(result,indent=2))
