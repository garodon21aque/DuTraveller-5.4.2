"""Import the supplied rigged collection without replacing existing character assets."""
import sys, re, json, traceback, shutil
from pathlib import Path
import unreal
sys.path.insert(0, str(Path(__file__).parent))
import MeshyRetargetCore as M
root = Path(unreal.Paths.project_dir())
source = Path('\\\\?\\C:\\Users\\sabea\\Documents\\DuTraveller Assets\\Characters')
report = {'characters': [], 'parents': {}}
output = root / 'Saved/Automation/CharacterCollectionImport.json'
def write():
    output.write_text(json.dumps(report, indent=2, default=str))
for path in []:  # Parent inventory is established; avoid loading motion-matching databases during import.
    bp = M.load(path)
    cdo = unreal.get_default_object(bp.generated_class())
    comp = cdo.get_editor_property('mesh')
    mesh = comp.get_editor_property('skeletal_mesh_asset')
    report['parents'][path] = {'mesh': M.name_of(mesh), 'anim_class': M.name_of(comp.get_editor_property('anim_class')), 'analysis': M.analyze_skeletal_mesh(mesh)}
for fbx in sorted(source.rglob('*animated*.fbx')):
    name = re.sub(r'_\d{8}_.*', '', fbx.stem.removeprefix('Meshy_AI_')).replace('_', '')
    row = {'name': name, 'source': str(fbx)}
    report['characters'].append(row)
    try:
        staging = root / 'SourceAssets/Characters' / name
        staging.mkdir(parents=True, exist_ok=True)
        for item in fbx.parent.iterdir():
            if item.is_file() and item.suffix.lower() in ('.png', '.jpg', '.jpeg'):
                shutil.copy2(item, staging / item.name)
        staged_fbx = staging / (name + '.fbx')
        shutil.copy2(fbx, staged_fbx)
        row['staged_source'] = str(staged_fbx)
        dest = '/Game/DuTraveller/Characters/Collection/' + name
        meshpath = dest + '/SK_Meshy_' + name
        if M.exists(meshpath):
            row['import'] = {'mesh': meshpath, 'analysis': M.analyze_skeletal_mesh(meshpath)}
        else:
            row['import'] = M.import_meshy_fbx(str(staged_fbx), name, dest_dir=dest)
        row['passed'] = bool(row['import'].get('mesh'))
    except Exception:
        row['error'] = traceback.format_exc()
    write()
    unreal.log('COLLECTION_IMPORTED ' + name)
write()
unreal.SystemLibrary.quit_editor()
