import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())
audit={}
for path in ('/Game/Blueprints/Framework/BP_PlayerController','/Game/Blueprints/AI/BP_CombatTest_Enemy'):
    bp=unreal.load_asset(path); cdo=unreal.get_default_object(bp.generated_class())
    props=('hud_widget_class','inventory_widget_class','panel_classes','ui_only_input','block_gameplay_while_panel_open') if 'PlayerController' in path else ('animation_router',)
    audit[path]={p:str(cdo.get_editor_property(p)) for p in props}
    if 'Enemy' in path:
        r=cdo.get_editor_property('animation_router'); audit[path]['hit_reaction']=str(r.get_editor_property('hit_reaction'))
(root/'Saved/Automation/MenuHitPathBaseline-Assets.json').write_text(json.dumps(audit,indent=2))
source=(root/'Tools/Run-TutorialLootUI-PIE.py').read_text()
source=source.replace('TutorialLootUI-PIE.json','MenuHitPathBaseline.json')
exec(compile(source,'Run-TutorialLootUI-PIE.py','exec'))
