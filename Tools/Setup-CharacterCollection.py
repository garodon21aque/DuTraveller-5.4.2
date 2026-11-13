"""Build player/enemy retarget routes and validate imported skeletal assets."""
import sys, json, traceback
from pathlib import Path
import unreal
sys.path.insert(0, str(Path(__file__).parent))
import MeshyRetargetCore as M
M.RIG_ROOT = '/Game/DuTraveller/Animations/Collection/IKRigs'
M.RTG_ROOT = '/Game/DuTraveller/Animations/Collection/Retargeters'
M.OUT_ROOT = '/Game/DuTraveller/Animations/Collection/Retargeted'
root = Path(unreal.Paths.project_dir())
imports = json.loads((root / 'Saved/Automation/CharacterCollectionImport.json').read_text())
report = {'characters': [], 'passed': False}
out = root / 'Saved/Automation/CharacterCollectionSetup.json'
def write():
    out.write_text(json.dumps(report, indent=2, default=str))
for entry in imports['characters']:
    name = entry['name']
    row = {'name': name, 'passed': False}
    report['characters'].append(row)
    try:
        meshpath = entry['import']['mesh']
        mesh = M.load(meshpath)
        row['mesh'] = meshpath
        row['analysis'] = M.analyze_skeletal_mesh(mesh)
        row['materials'] = [M.name_of(x.material_interface) for x in mesh.get_editor_property('materials')]
        if name == 'GildedWarhorse':
            row['role'] = 'quadruped; retain native skeleton and supplied walking animation'
            row['passed'] = row['analysis']['rigged'] and bool(row['materials'])
        else:
            assert row['analysis']['humanoid'], 'Missing required humanoid chains'
            key = 'Collection' + name
            pipeline = M.run_pipeline(name=key, mesh_path=meshpath, routes=('A', 'C'),
                                      retarget_library=True, create_enemy=False, adapt_animbp=False)
            row['pipeline'] = pipeline
            assert pipeline['passed'], str(pipeline['errors'])
            animpath, animreport = M.adapt_animbp(M.ENEMY_ANIMBP,
                M.OUT_ROOT + '/' + key + '/ABP_' + key + '_Enemy', row['analysis']['skeleton'],
                prefix='DT_', suffix='_Meshy_' + key, name=key)
            row['enemy_animbp'] = animpath
            row['animbp_report'] = animreport
            row['validation'] = M.validate(name=key, mesh_path=meshpath, anim_bp=animpath)
            outputs = [o for r in pipeline['stages']['retargets'].values() for o in r.get('outputs', [])]
            row['animation_count'] = len(outputs)
            row['passed'] = (row['validation']['passed'] and bool(outputs)
                             and all(o['skeleton_match'] for o in outputs)
                             and all(row['materials']))
    except Exception:
        row['error'] = traceback.format_exc()
    write()
    unreal.log('COLLECTION_SETUP ' + name + ' ' + str(row['passed']))
report['passed'] = len(report['characters']) == 20 and all(x['passed'] for x in report['characters'])
write()
unreal.SystemLibrary.quit_editor()
