"""Read/export only. Run in the isolated probe whose Content points at the old library.
Never compile or save any source asset from that library.
"""
import json
from pathlib import Path
import unreal

out = Path(r'C:/Personal Games/DuTraveller-5.4.2-source/Saved/Automation/OlderLocalLibrary')
out.mkdir(parents=True, exist_ok=True)
report = {'source': r'D:\GameJams\Github\DuTraveller', 'tables': [], 'items': []}
table = unreal.load_asset('/Game/Blueprints/Quest/QuestData')
if table:
    raw = unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table)
    (out / 'OriginalQuestData.json').write_text(raw, encoding='utf-8')
    report['tables'].append({'path': table.get_path_name(), 'rows': [str(x) for x in unreal.DataTableFunctionLibrary.get_data_table_row_names(table)], 'struct': table.get_editor_property('row_struct').get_path_name()})
paths = unreal.EditorAssetLibrary.list_assets('/Game/Blueprints/InventorySystem/Items', recursive=True, include_folder=False)
for path in paths:
    if 'Equipment' not in path and 'Gear' not in path:
        continue
    asset = unreal.load_asset(path)
    if not isinstance(asset, unreal.Blueprint):
        continue
    cdo = unreal.get_default_object(asset.generated_class())
    entry = {'path': path, 'parent': unreal.BlueprintEditorLibrary.get_blueprint_parent_class(asset).get_path_name()}
    for key in ('slot', 'item_display_name', 'thumbnail', 'attribute_bonuses'):
        try:
            entry[key] = str(cdo.get_editor_property(key))
        except Exception:
            pass
    report['items'].append(entry)
(out / 'ReadOnlyAudit.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('OLDER_LIBRARY_AUDIT ' + json.dumps(report))
