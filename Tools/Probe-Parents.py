import unreal
ar=unreal.AssetRegistryHelpers.get_asset_registry()
targets=['/Game/UI/World/WBP_XPPopUp','/Game/UI/World/WBP_LevelUpNotification','/Game/UI/QuestSystem/WBP_Objectives','/Game/UI/QuestSystem/WBP_QuestLog_QuestEntry','/Game/UI/InventorySystem/WBP_ItemObject','/Game/UI/InventorySystem/WBP_VendorItemObject','/Game/UI/HUD/SpellGlobes/WBP_SpellGlobe','/Game/UI/QuestSystem/WBP_QuestRewards','/Game/UI/World/WBP_EnemyHealthWidget','/Game/UI/World/WPB_TargetEnemy','/Game/UI/World/WBP_ItemInteract','/Game/UI/QuestSystem/WBP_QuestLog','/Game/UI/QuestSystem/WBP_QuestGiver']
for t in targets:
    ad=ar.get_asset_by_object_path(t)
    parent=None
    if ad and ad.is_valid():
        try:
            parent=ad.get_tag_value('ParentClass')
        except Exception as e:
            parent='tagerr %s'%e
    print(t,'| parent:',parent)
unreal.exit(0)
