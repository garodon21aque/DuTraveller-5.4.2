"""Focused rendered PIE checks: genuine level events/popups and persistent roll endpoints."""
import unreal,json,time,traceback,math
from pathlib import Path
root=Path(unreal.Paths.project_dir()); out=root/'Saved/Automation/XPAndRollPIE.json'
report={'passed':False,'checks':[],'errors':[],'progression':[],'rolls':[],'runtime':{}}
worlds=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem); editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem); helper=unreal.DTStructRepairLibrary
pc=pawn=ps=overlay=observer=bar=enemy=None; flags=set(); rows={}; seen_popups={}; callback=regression_callback=None; last_world=None; clock=0.; last_sample=-1; started=time.monotonic()
directions=[(-1,0),(1,0),(0,1),(0,-1),(1,1),(1,-1),(-1,1),(-1,-1)]
def valid(v): return v is not None and unreal.SystemLibrary.is_valid(v)
def vec(v): return [v.x,v.y,v.z]
def distance(a,b): return sum((a[i]-b[i])**2 for i in range(3))**.5
def save(): out.write_text(json.dumps(report,indent=2,default=str))
def check(name,value): report['checks'].append({'name':name,'passed':bool(value)}); save()
def capture():
    mesh=pawn.get_editor_property('mesh'); anim=mesh.get_anim_instance(); capsule=pawn.get_editor_property('capsule_component')
    return {'time':clock,'actor':vec(pawn.get_actor_location()),'capsule':vec(capsule.get_world_location()),'mesh_world':vec(mesh.get_world_location()),'mesh_relative':vec(mesh.get_editor_property('relative_location')),'root_world':vec(mesh.get_socket_location('root')),'pelvis_world':vec(mesh.get_socket_location('pelvis')),'head_component':vec(mesh.get_socket_transform('head',unreal.RelativeTransformSpace.RTS_COMPONENT).translation),'dodging':pawn.get_editor_property('bIsDodging'),'montage':str(anim.get_current_active_montage()),'root_mode':str(anim.get_editor_property('root_motion_mode'))}
def notifications(world):
    for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.UserWidget,False):
        if 'WBP_LevelUpNotification' in w.get_class().get_name(): seen_popups[w.get_path_name()]=int(w.get_editor_property('Level'))
    return len(seen_popups)
def progress(world,label):
    row={'stage':label,'level':ps.get_editor_property('LVL'),'xp':ps.get_editor_property('EXP'),'percent':ps.get_experience_percent(),'bar_percent':bar.get_editor_property('percent'),'state_events':list(observer.get_editor_property('player_state_levels')),'hud_level_events':list(observer.get_editor_property('overlay_levels')),'popup_count':notifications(world),'popup_levels':list(seen_popups.values())}
    row['hud_level_texts']=[{'path':w.get_path_name(),'text':str(w.get_text())} for w in helper.get_runtime_ui_widgets(pc.get_editor_property('hud_widget')) if isinstance(w,unreal.TextBlock) and w.get_name()=='Text_Value']
    report['progression'].append(row); save(); return row
def finish():
    global regression_callback
    if callback is not None: unreal.unregister_slate_post_tick_callback(callback)
    report['passed']=not report['errors'] and bool(report['checks']) and all(c['passed'] for c in report['checks']); save()
    editor.editor_request_end_play()
    def regression(delta):
        if worlds.get_game_world(): return
        unreal.unregister_slate_post_tick_callback(regression_callback)
        unreal.SystemLibrary.execute_console_command(worlds.get_editor_world(),'Automation RunTests DuTraveller.Restoration')
    regression_callback=unreal.register_slate_post_tick_callback(regression)
def tick(delta):
    global pc,pawn,ps,overlay,observer,bar,enemy,last_world,clock,last_sample
    try:
        if time.monotonic()-started>220: raise RuntimeError('XP/roll PIE timeout')
        world=worlds.get_game_world()
        if not world: return
        now=unreal.GameplayStatics.get_time_seconds(world)
        pc=unreal.GameplayStatics.get_player_controller(world,0); pawn=pc.get_controlled_pawn() if pc else None
        if not pawn or now<1: return
        for e in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RPGAICharacter):
            if e.get_controller(): e.get_controller().set_actor_tick_enabled(False); e.get_controller().stop_movement()
            e.get_editor_property('action_comp').cancel_pending_release('XP/root-motion fixture')
            if 'BP_CombatTest_Enemy' in e.get_class().get_name() and not e.get_editor_property('bIsDead'): enemy=e
        if 'init' not in flags:
            ps=pc.get_editor_property('player_state'); overlay=unreal.RPG_AbilitySystemLibrary.get_overlay_widget_controller(pc); observer=helper.observe_progression(ps,overlay)
            widgets=helper.get_runtime_ui_widgets(pc.get_editor_property('hud_widget'))
            globe=next(w for w in widgets if 'WBP_ExperienceGlobe' in w.get_class().get_name())
            bar=next(w for w in helper.get_runtime_ui_widgets(globe) if isinstance(w,unreal.ProgressBar))
            pc.set_control_rotation(unreal.Rotator(pitch=0,yaw=0,roll=0)); last_world=now
            report['runtime']=dict(helper.inspect_runtime_player(pc)); report['runtime']['root_motion_mode']=str(pawn.get_editor_property('mesh').get_anim_instance().get_editor_property('root_motion_mode'))
            report['runtime']['dodge_montage']=str(pawn.get_editor_property('dodge_animation').get_editor_property('montage'))
            report['runtime']['hud_widgets']=[{'name':w.get_name(),'class':w.get_class().get_name(),'text':str(w.get_text()) if hasattr(w,'get_text') else None} for w in widgets if isinstance(w,unreal.TextBlock)]
            check('Fresh PIE starts at level one with no level-up popup',ps.get_editor_property('LVL')==1 and ps.get_editor_property('EXP')==0 and notifications(world)==0)
            flags.add('init')
        clock+=now-last_world; last_world=now
        if clock>2 and 'partial' not in flags:
            check('Partial XP award accepted',ps.grant_experience(24,False)); row=progress(world,'24 XP')
            check('Ordinary XP updates the real HUD bar without a level-up',row['level']==1 and row['xp']==24 and abs(row['bar_percent']-.24)<.001 and row['state_events']==[] and row['hud_level_events']==[] and row['popup_count']==0); flags.add('partial')
        if clock>4 and 'threshold' not in flags:
            ps.grant_experience(76,False); row=progress(world,'exact 100 XP')
            check('Full XP threshold advances level and resets the HUD bar',row['level']==2 and row['xp']==0 and row['bar_percent']==0)
            check('One genuine level creates exactly one event and popup',row['state_events']==[2] and row['hud_level_events']==[2] and row['popup_levels']==[2]); flags.add('threshold')
        if clock>6 and 'multiple' not in flags:
            ps.grant_experience(250,False); row=progress(world,'250 XP award')
            check('One award gains multiple levels and preserves overflow',row['level']==4 and row['xp']==50 and abs(row['bar_percent']-.5)<.001)
            check('Each gained level produces one event and one genuine popup',row['state_events']==[2,3,4] and row['hud_level_events']==[2,3,4] and sorted(row['popup_levels'])==[2,3,4]); flags.add('multiple')
        if clock>8 and 'points' not in flags:
            ps.grant_progression_points(3,2); overlay.broadcast_initial_values(); ps.grant_experience(0,False); row=progress(world,'points/rebind/zero XP')
            check('Point changes and repeated HUD initialization do not create level-ups',row['state_events']==[2,3,4] and row['hud_level_events']==[2,3,4] and row['popup_count']==3); flags.add('points')
        if clock>10 and 'kill' not in flags:
            if not valid(enemy): raise RuntimeError('No actual Tutorial enemy for XP reward verification')
            before_level=ps.get_editor_property('LVL'); before_xp=ps.get_editor_property('EXP'); reward=enemy.get_editor_property('experience_reward'); threshold=ps.get_editor_property('required_xp')
            enemy.receive_combat_damage(10000,pawn,'Test.XPReward'); row=progress(world,'actual enemy death')
            levels=int((before_xp+reward)//threshold)
            check('Actual enemy death grants XP and resolves the same thresholds',row['level']==before_level+levels and abs(row['xp']-((before_xp+reward)%threshold))<.001)
            check('Enemy reward does not generate an unrelated level-up popup',row['popup_count']==3+levels); flags.add('kill')
        for index,(x,y) in enumerate(directions):
            at=12+index*2.5
            if clock>=at and index not in rows:
                before=capture(); direction=unreal.Vector(x/math.sqrt(x*x+y*y),y/math.sqrt(x*x+y*y),0)
                if index==0:
                    helper.inject_runtime_input(pc,'LeftAlt',unreal.Vector(1,0,0)); helper.inject_runtime_input(pc,'LeftAlt',unreal.Vector()); accepted=pawn.get_editor_property('bIsDodging')
                else: accepted=pawn.request_dodge(direction)
                rows[index]={'index':index,'direction':vec(direction),'start':before,'samples':[],'landed':None}
                check('Roll '+str(index)+' starts with the corrected root-motion montage',accepted and 'RootMotion' in capture()['montage'])
            if index in rows and clock<at+1.8:
                row=rows[index]; sample=capture(); row['samples'].append(sample)
                if not sample['dodging'] and row['landed'] is None and clock>at+.15: row['landed']=sample
            if index in rows and clock>at+1.8 and 'roll_done_'+str(index) not in flags:
                row=rows[index]; end=capture(); row['end']=end; start=row['start']; d=row['direction']; row['capsule_travel_along_direction']=sum((end['capsule'][i]-start['capsule'][i])*d[i] for i in range(3))
                landed=row['landed']; row['capsule_drift_after_landing']=distance(end['capsule'],landed['capsule']) if landed else None; row['pelvis_drift_after_landing']=distance(end['pelvis_world'],landed['pelvis_world']) if landed else None
                check('Roll '+str(index)+' capsule reaches and retains a new endpoint',row['capsule_travel_along_direction']>200 and not end['dodging'] and landed is not None and row['capsule_drift_after_landing']<3)
                check('Roll '+str(index)+' visible mesh stays at the landed position',landed is not None and row['pelvis_drift_after_landing']<25 and distance(end['actor'],end['capsule'])<.1 and distance(start['mesh_relative'],end['mesh_relative'])<.1)
                last_active=next((s for s in reversed(row['samples']) if s['dodging']),None)
                row['visible_endpoint_gap_cm']=sum((end['pelvis_world'][i]-last_active['pelvis_world'][i])**2 for i in [0,1])**.5 if last_active else None
                check('Roll '+str(index)+' does not snap the mesh back at montage blend-out',last_active is not None and row['visible_endpoint_gap_cm']<50)
                check('Roll '+str(index)+' tumbles instead of playing an upright pose',min(s['head_component'][2] for s in row['samples'])<100)
                report['rolls'].append(row); flags.add('roll_done_'+str(index)); save()
        if clock>33:
            check('The controller still accepts walking after root-motion rolls',not pc.is_move_input_ignored())
            finish()
    except Exception:
        report['errors'].append(traceback.format_exc()); unreal.log_error(report['errors'][-1]); finish()
try:
    prep_path=root/'Saved/Automation/XPRollPreparation.json'
    if not prep_path.exists() or not json.loads(prep_path.read_text())['passed']:
        exec(compile((root/'Tools/Prepare-XPRoll.py').read_text(),'Prepare-XPRoll.py','exec'),{})
    else: report['reused_verified_asset_preparation']=True
    if not json.loads((root/'Saved/Automation/XPRollPreparation.json').read_text())['passed']: raise RuntimeError('XP/root-motion asset preparation failed')
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    unreal.EditorLoadingAndSavingUtils.load_map('/Game/Levels/Tutorial'); callback=unreal.register_slate_post_tick_callback(tick); editor.editor_request_begin_play()
except Exception:
    report['errors'].append(traceback.format_exc()); save(); unreal.EditorPythonScripting.set_keep_python_script_alive(False)


