import unreal
lib=unreal.BlueprintEditorLibrary
for p in ['/Game/UI/InventorySystem/WBP_PlayerStat','/Game/UI/World/WBP_ItemInteract','/Game/UI/InventorySystem/WBP_DropWidget','/Game/UI/HUD/WBP_HUD','/Game/UI/World/WPB_TargetEnemy','/Game/Blueprints/Player/BP_RPGPlayerController','/Game/UI/InventorySystem/WBP_ItemObject','/Game/UI/InventorySystem/WBP_VendorItemObject','/Game/UI/HUD/SpellGlobes/WBP_SpellGlobe','/Game/UI/World/WBP_XPPopUp','/Game/UI/World/WBP_LevelUpNotification','/Game/UI/QuestSystem/WBP_Objectives','/Game/UI/QuestSystem/WBP_QuestLog_QuestEntry','/Game/UI/InventorySystem/WBP_NPCVendorShop','/Game/UI/QuestSystem/WBP_QuestLog','/Game/UI/QuestSystem/WBP_QuestGiver','/Game/UI/QuestSystem/WBP_QuestRewards','/Game/UI/InventorySystem/WBP_PlayerInventory']:
    try:
        bp=unreal.load_asset(p)
        vs=[n for n in lib.list_member_variable_names(bp) if not str(n).startswith('/Script')]
        print(p.split('/')[-1],'|',vs)
    except Exception as e:
        print(p,'ERR',e)
unreal.SystemLibrary.quit_editor()
