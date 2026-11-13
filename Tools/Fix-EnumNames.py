import unreal
RN = unreal.DTStructRepairLibrary.set_user_defined_enum_entry_names
for path,names in [("/Game/Blueprints/Quest/EQuestTypes.EQuestTypes",["Main","Side","Renown","Dungeon","Class","Bonus"]),
                   ("/Game/Blueprints/Quest/EQObjctiveTypes.EQObjctiveTypes",["Interact","Collect","Kill","Talk","Location"])]:
    print(path, RN(path,names))
    a=unreal.load_asset(path.split('.')[0])
    if a: unreal.EditorAssetLibrary.save_loaded_asset(a, only_if_is_dirty=False); print('saved')
unreal.SystemLibrary.quit_editor()
