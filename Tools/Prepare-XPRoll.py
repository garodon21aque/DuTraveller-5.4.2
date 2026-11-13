import unreal,json,shutil,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir()); result={'passed':False,'assets':[],'errors':[]}
try:
    for rel in ['Content/Blueprints/Characters/BP_PlayerCharacter.uasset','Content/Blueprints/Animations/ABP_SandboxCharacter.uasset']:
        original=root/rel; saved=root/'Saved/RestorationBackups/XPRoll'/rel
        if not saved.exists(): saved.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(original,saved)
    path='/Game/UI/HUD/WBP_HUD'; source=root/'Content/UI/HUD/WBP_HUD.uasset'; backup=root/'Saved/RestorationBackups/XPRoll/Content/UI/HUD/WBP_HUD.uasset'
    if not backup.exists(): backup.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(source,backup)
    helper=unreal.DTStructRepairLibrary
    if not helper.repair_hud_level_up(path) or not helper.force_compile_and_save(path): raise RuntimeError('HUD level-up flag branch failed')
    result['assets'].append(path); helper.dump_blueprint_graph(path)
    exec(compile((root/'Tools/Prepare-DodgeFocus.py').read_text(),'Prepare-DodgeFocus.py','exec'),{})
    dodge=json.loads((root/'Saved/Automation/DodgeFocusPreparation.json').read_text())
    if not dodge['passed']: raise RuntimeError('Root-motion preparation failed: '+str(dodge['errors']))
    result['assets']+=dodge['assets']
    seq=unreal.load_asset('/Game/DTIntegration/Animation/Avoidance/DT_Stand_To_Roll_UEFN')
    result['root_motion']=seq.get_editor_property('enable_root_motion'); result['force_root_lock']=seq.get_editor_property('force_root_lock'); result['poses']=[]
    seq.set_editor_property('force_root_lock',False) # Read the raw moving root track, before extraction locks it.
    for f in [0,.2,.4,.6,.8,1]:
        opts=unreal.AnimPoseEvaluationOptions()
        pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(seq,seq.get_play_length()*f,opts)
        tr=unreal.AnimPoseExtensions.get_bone_pose(pose,'root',unreal.AnimPoseSpaces.WORLD)
        result['poses'].append({'fraction':f,'root_translation':[tr.translation.x,tr.translation.y,tr.translation.z],'root_rotation':str(tr.rotation)})
    seq.set_editor_property('force_root_lock',True)
    d=result['poses'][-1]['root_translation']; a=result['poses'][0]['root_translation']; result['root_displacement_cm']=sum((d[i]-a[i])**2 for i in [0,1])**.5
    if result['root_displacement_cm']<200: raise RuntimeError('Retarget lost the moving root track')
    result['passed']=True
except Exception: result['errors'].append(traceback.format_exc()); unreal.log_error(result['errors'][-1])
(root/'Saved/Automation/XPRollPreparation.json').write_text(json.dumps(result,indent=2))


