import unreal
lib=unreal.BlueprintEditorLibrary
for path in ['/Game/UI/World/WBP_ItemInteract','/Game/UI/InventorySystem/WBP_PlayerStat','/Game/Blueprints/Quest/BP_QuestGiver','/Game/UI/World/WPB_TargetEnemy','/Game/UI/HUD/WBP_HUD']:
    bp=unreal.load_asset(path)
    print('==',path)
    for n in lib.list_member_variable_names(bp):
        try:
            t=lib.get_member_variable_type(bp,n)
            print('  ',n,'->',t.export_text()[:160])
        except Exception as e:
            print('  ',n,'-> ERR',e)
