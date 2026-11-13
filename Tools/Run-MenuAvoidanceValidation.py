import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())
exec(compile((root/'Tools/Prepare-MenuAvoidanceReactions.py').read_text(),'Prepare-MenuAvoidanceReactions.py','exec'),{})
if not json.loads((root/'Saved/Automation/MenuAvoidanceReactionPreparation.json').read_text())['passed']: raise RuntimeError('Asset preparation failed')
source=(root/'Tools/Run-DodgeFocusPIE.py').read_text()
start=source.index('def finish():')
end=source.index('\ndef tick(delta):',start)
replacement='''def finish():
    global regression_callback
    unreal.unregister_slate_post_tick_callback(callback)
    if valid(pawn):
        pawn.set_focus_held(False)
        pawn.get_editor_property("mesh").set_visibility(True,True)
    report["passed"]=not report["errors"] and bool(report["checks"]) and all(c["passed"] for c in report["checks"]); save()
    editor.editor_request_end_play()
    def next_probe(delta):
        if worlds.get_game_world(): return
        unreal.unregister_slate_post_tick_callback(regression_callback)
        ui=(root/"Tools/Run-EnemyReactionPIE.py").read_text()
        exec(compile(ui,"Run-TutorialLootUI-PIE.py","exec"),{})
    regression_callback=unreal.register_slate_post_tick_callback(next_probe)
'''
source=source[:start]+replacement+source[end:]
exec(compile(source,'Run-DodgeFocusPIE.py','exec'),{})

