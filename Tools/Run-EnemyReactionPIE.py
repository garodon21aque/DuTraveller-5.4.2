"""Actual player melee/projectile attacks must visibly drive the live enemy hit reaction."""
import unreal,json,time,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir()); out=root/'Saved/Automation/EnemyHitReactionsPIE.json'
report={'passed':False,'checks':[],'errors':[],'samples':[]}; flags=set(); callback=next_callback=None; started=time.monotonic(); last=-1; pc=pawn=enemy=None
worlds=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem); editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem); probe=unreal.DTStructRepairLibrary
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def save(): out.write_text(json.dumps(report,indent=2,default=str))
def check(name,ok): report['checks'].append({'name':name,'passed':bool(ok)}); save()
def finish():
    global next_callback
    unreal.unregister_slate_post_tick_callback(callback); report['passed']=not report['errors'] and bool(report['checks']) and all(c['passed'] for c in report['checks']); save(); editor.editor_request_end_play()
    def next_probe(delta):
        if worlds.get_game_world(): return
        unreal.unregister_slate_post_tick_callback(next_callback)
        ui=(root/'Tools/Run-TutorialLootUI-PIE.py').read_text().replace('TutorialLootUI-PIE.json','MenuClickVerification.json')
        exec(compile(ui,'Run-TutorialLootUI-PIE.py','exec'),{})
    next_callback=unreal.register_slate_post_tick_callback(next_probe)
def tick(delta):
    global pc,pawn,enemy,last
    try:
        if time.monotonic()-started>150: raise RuntimeError('Enemy reaction PIE timeout')
        world=worlds.get_game_world()
        if not world: return
        t=unreal.GameplayStatics.get_time_seconds(world)
        pc=unreal.GameplayStatics.get_player_controller(world,0); pawn=pc.get_controlled_pawn() if pc else None
        if not pawn or t<1: return
        if 'init' not in flags:
            for e in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RPGAICharacter):
                if e.get_controller(): e.get_controller().set_actor_tick_enabled(False); e.get_controller().stop_movement()
                e.get_editor_property('action_comp').cancel_pending_release('reaction fixture')
                e.get_editor_property('animation_router').stop_active_montage(.05)
                if 'BP_CombatTest_Enemy' in e.get_class().get_name(): enemy=e
            if not enemy: raise RuntimeError('Missing actual Tutorial enemy')
            for shot in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.RPGProjectile): shot.destroy_actor()
            pc.set_control_rotation(unreal.Rotator(pitch=0,yaw=0,roll=0)); pawn.set_actor_rotation(unreal.Rotator(pitch=0,yaw=0,roll=0),True)
            enemy.set_actor_location(pawn.get_actor_location()+unreal.Vector(110,40,0),False,True)
            report['initial_health']=probe.read_runtime_attribute(enemy,'Health'); flags.add('init')
        if t>1.3 and 'melee' not in flags:
            probe.inject_runtime_input(pc,'LeftMouseButton',unreal.Vector(1,0,0)); probe.inject_runtime_input(pc,'LeftMouseButton',unreal.Vector()); flags.add('melee')
        hp=probe.read_runtime_attribute(enemy,'Health'); anim=enemy.get_editor_property('mesh').get_anim_instance(); montage=anim.get_current_active_montage()
        if t<3.5 and hp<report['initial_health'] and montage and 'AM_EnemyHitReaction' in montage.get_name():
            if 'melee_hit' not in flags:
                check('Player melee damages the real enemy and plays its hit montage',True)
                enemy.get_controller().run_combat_decision()
                check('AI decision cannot immediately replace its hit reaction',anim.get_current_active_montage()==montage); flags.add('melee_hit')
        if t>3.5 and 'spell' not in flags:
            enemy.get_editor_property('action_comp').cancel_pending_release('spell fixture')
            enemy.set_actor_location(pawn.get_actor_location()+unreal.Vector(350,0,0),False,True)
            report['before_spell_health']=hp
            probe.inject_runtime_input(pc,'One',unreal.Vector(1,0,0)); probe.inject_runtime_input(pc,'One',unreal.Vector()); flags.add('spell')
        if 'spell' in flags and hp<report['before_spell_health'] and montage and 'AM_EnemyHitReaction' in montage.get_name(): flags.add('spell_hit')
        if t-last>.05:
            mesh=enemy.get_editor_property('mesh'); pose=mesh.get_socket_transform('head',unreal.RelativeTransformSpace.RTS_COMPONENT).translation
            report['samples'].append({'time':t,'health':hp,'montage':str(montage),'head':[pose.x,pose.y,pose.z]}); last=t
        if t>7:
            check('Melee hit was observed','melee_hit' in flags); check('Montage-notify projectile damage plays enemy hit reaction','spell_hit' in flags)
            values={tuple(round(x,1) for x in s['head']) for s in report['samples'] if 'AM_EnemyHitReaction' in s['montage']}
            check('Enemy hit montage changes the rendered skeletal pose',len(values)>3); finish()
    except Exception: report['errors'].append(traceback.format_exc()); unreal.log_error(report['errors'][-1]); finish()
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Levels/Tutorial'); callback=unreal.register_slate_post_tick_callback(tick); editor.editor_request_begin_play()

