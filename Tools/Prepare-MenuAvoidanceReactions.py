"""Configure existing UI and enemy reaction assets; preserve originals in a backup."""
import unreal,json,shutil,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir()); result={'passed':False,'assets':[],'errors':[]}; helper=unreal.DTStructRepairLibrary

def backup(path):
    src=root/('Content/'+path.removeprefix('/Game/')+'.uasset'); dst=root/'Saved/RestorationBackups/MenuAvoidanceReactions'/src.relative_to(root)
    if src.exists() and not dst.exists(): dst.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(src,dst)
def savebp(path):
    if not helper.force_compile_and_save(path): raise RuntimeError('Compile/save failed: '+path)
    result['assets'].append(path)
try:
    path='/Game/Blueprints/Framework/BP_PlayerController'; backup(path)
    bp=unreal.load_asset(path); cdo=unreal.get_default_object(bp.generated_class())
    menu_paths={'Inventory':'/Game/UI/InventorySystem/WBP_PlayerInventory','Crafting':'/Game/UI/CraftingSystem/WBP_CraftingMenu','Party':'/Game/UI/Party/WBP_PartyMenu','Vendor':'/Game/UI/InventorySystem/WBP_NPCVendorShop','Skills':'/Game/UI/SpellMenu/WBP_SpellMenu','Talents':'/Game/UI/HUD/WBP_Talents','Attributes':'/Game/UI/Attribute/WBP_AttributeMenu','Quests':'/Game/UI/QuestSystem/WBP_QuestLog','Pause':'/Game/UI/Menu/WBP_PauseMenu','Settings':'/Game/UI/Menu/WBP_Settings'}
    classes={k:unreal.load_class(None,v+'.'+v.rsplit('/',1)[1]+'_C') for k,v in menu_paths.items()}
    if not all(classes.values()): raise RuntimeError('Missing menu class: '+str(classes))
    cdo.set_editor_property('panel_classes',classes)
    for prop,name in [('inventory_widget_class','Inventory'),('crafting_widget_class','Crafting'),('party_widget_class','Party'),('vendor_widget_class','Vendor')]: cdo.set_editor_property(prop,classes[name])
    cdo.set_editor_property('hud_widget_class',unreal.load_class(None,'/Game/UI/HUD/WBP_HUD.WBP_HUD_C'))
    cdo.set_editor_property('ui_only_input',True); cdo.set_editor_property('block_gameplay_while_panel_open',True)
    if not helper.expose_controller_menu_flow(path): raise RuntimeError('Could not expose controller menu graph')
    savebp(path); helper.dump_blueprint_graph(path)
    reaction_path='/Game/DTIntegration/Animation/Combat/AM_EnemyHitReaction'
    reaction=unreal.load_asset(reaction_path)
    if not reaction: reaction=unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset('AM_EnemyHitReaction','/Game/DTIntegration/Animation/Combat',unreal.load_asset('/Game/Art/3D/Ai/Enemy/Anims/AMAI_ReactHitFont'))
    if not isinstance(reaction,unreal.AnimMontage): raise RuntimeError('Enemy reaction source is not a montage')
    reaction.set_editor_property('enable_auto_blend_out',True)
    if not helper.set_montage_slot_name(reaction_path,'Upperbody'): raise RuntimeError('Reaction slot configuration failed')
    unreal.EditorAssetLibrary.save_loaded_asset(reaction,False); result['assets'].append(reaction_path)
    for path in ['/Game/Blueprints/AI/BP_CombatTest_Enemy','/Game/Blueprints/AI/BP_RPGAICharacter','/Game/Blueprints/AI/BP_AIRangedCharacter']:
        bp=unreal.load_asset(path)
        if not bp: continue
        cdo=unreal.get_default_object(bp.generated_class()); mesh=cdo.get_editor_property('mesh'); sk=mesh.get_editor_property('skeletal_mesh_asset')
        if not sk or sk.get_editor_property('skeleton')!=reaction.get_editor_property('skeleton'): continue
        backup(path); entry=unreal.DTCombatAnimation(); entry.set_editor_property('montage',reaction); entry.set_editor_property('play_rate',1.)
        cdo.get_editor_property('animation_router').set_editor_property('hit_reaction',entry)
        savebp(path)
    roll=unreal.load_asset('/Game/DTIntegration/Animation/Avoidance/DT_Stand_To_Roll_inplace_mixamo_com_UEFN')
    backup('/Game/DTIntegration/Animation/Avoidance/DT_Stand_To_Roll_inplace_mixamo_com_UEFN')
    # The recovered in-place roll animates root rotation. Locking its root strips
    # the somersault from the visible pose even while the montage is advancing.
    roll.set_editor_property('force_root_lock',False); roll.set_editor_property('enable_root_motion',False)
    unreal.EditorAssetLibrary.save_loaded_asset(roll,False)
    result['assets'].append(roll.get_path_name())
    montage=unreal.load_asset('/Game/DTIntegration/Animation/Avoidance/AM_DT_DodgeRoll')
    backup('/Game/DTIntegration/Animation/Avoidance/AM_DT_DodgeRoll')
    for prop,duration in [('blend_in',.08),('blend_out',.1)]:
        blend=montage.get_editor_property(prop); blend.set_editor_property('blend_time',duration); montage.set_editor_property(prop,blend)
    unreal.EditorAssetLibrary.save_loaded_asset(montage,False); result['assets'].append(montage.get_path_name())
    result['roll_pose_audit']=[]
    for seq in [unreal.load_asset('/Game/Art/3D/Player/Animations/Anims/Retargeted/Stand_To_Roll_inplace_mixamo_com'),roll]:
        for fraction in [0,.2,.4,.6,.8,1]:
            pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(seq,seq.get_play_length()*fraction,unreal.AnimPoseEvaluationOptions())
            result['roll_pose_audit'].append({'asset':seq.get_path_name(),'fraction':fraction,'root':str(unreal.AnimPoseExtensions.get_bone_pose(pose,'root',unreal.AnimPoseSpaces.WORLD)),'head':str(unreal.AnimPoseExtensions.get_bone_pose(pose,'head',unreal.AnimPoseSpaces.WORLD))})
    result['passed']=True
except Exception: result['errors'].append(traceback.format_exc()); unreal.log_error(result['errors'][-1])
(root/'Saved/Automation/MenuAvoidanceReactionPreparation.json').write_text(json.dumps(result,indent=2))

