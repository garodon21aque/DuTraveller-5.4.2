import unreal
bp=unreal.load_asset('/Game/UI/World/WBP_XPPopUp')
gc=bp.generated_class()
print([a for a in dir(gc) if 'super' in a.lower() or 'parent' in a.lower() or 'child' in a.lower()])
print([a for a in dir(unreal.Class) if 'super' in a.lower() or 'parent' in a.lower()])
for t in ['/Game/UI/World/WBP_XPPopUp','/Game/UI/World/WBP_LevelUpNotification','/Game/UI/QuestSystem/WBP_Objectives','/Game/UI/QuestSystem/WBP_QuestLog_QuestEntry','/Game/UI/InventorySystem/WBP_ItemObject','/Game/UI/InventorySystem/WBP_VendorItemObject','/Game/UI/HUD/SpellGlobes/WBP_SpellGlobe','/Game/UI/QuestSystem/WBP_QuestRewards','/Game/UI/World/WBP_EnemyHealthWidget','/Game/UI/World/WPB_TargetEnemy','/Game/UI/World/WBP_ItemInteract','/Game/UI/QuestSystem/WBP_QuestLog','/Game/UI/QuestSystem/WBP_QuestGiver','/Game/UI/HUD/WBP_HUD']:
    bp=unreal.load_asset(t)
    gc=bp.generated_class()
    sup=gc.get_outer()  # try outer then check class
    print(t,'| outer:',sup)
unreal.exit(0)
