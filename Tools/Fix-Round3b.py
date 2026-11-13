import unreal
lib = unreal.BlueprintEditorLibrary
eal = unreal.EditorAssetLibrary
repair = unreal.DTStructRepairLibrary

# 1. Dump WBP_QuestRewards to see the GetQuestActor member parent.
repair.dump_blueprint_graph('/Game/UI/QuestSystem/WBP_QuestRewards.WBP_QuestRewards')

# 2. Create the serialized BP function 'GetQuestActor' on BP_QuestLog
#    (RPGQuestComponent-derived; space-named 'Array Element' param).
ok = repair.create_bp_function(
    '/Game/Blueprints/Quest/BP_QuestLog.BP_QuestLog',
    'GetQuestActor',
    [
        'in|QuestID|name|',
        'out|Array_Element|int|',
        'out|Array Element|object|/Game/Blueprints/Quest/BP_QuestGiver.BP_QuestGiver_C',
        'out|ReturnValue|object|/Script/DuTraveller.RPGQuest',
    ])
unreal.log('create_bp_function={0}'.format(ok))
repair.regenerate_blueprint_class(
    '/Game/Blueprints/Quest/BP_QuestLog.BP_QuestLog')

# 3. Link-preserving refresh on stale-pin assets.
targets = [
    '/Game/UI/QuestSystem/WBP_QuestLog.WBP_QuestLog',
    '/Game/UI/QuestSystem/WBP_QuestRewards.WBP_QuestRewards',
    '/Game/UI/QuestSystem/WBP_QuestLog_QuestEntry.WBP_QuestLog_QuestEntry',
    '/Game/UI/World/WPB_TargetEnemy.WPB_TargetEnemy',
    '/Game/UI/World/WBP_ItemInteract.WBP_ItemInteract',
    '/Game/UI/InventorySystem/WBP_PlayerInventory.WBP_PlayerInventory',
    '/Game/Art/3D/Player/Animations/Anims/AnimBP_RPGCharacter.AnimBP_RPGCharacter',
    '/Game/Art/3D/Player/Animations/Anims/AnimBP_RPGAICharacter.AnimBP_RPGAICharacter',
]
for obj in targets:
    try:
        repair.refresh_blueprint_nodes(obj)
        pkg = obj.split('.')[0]
        bp = unreal.load_asset(pkg)
        if bp:
            eal.save_loaded_asset(bp, only_if_is_dirty=False)
            unreal.log('saved ' + pkg)
    except Exception as exc:  # noqa: BLE001
        unreal.log_warning('refresh failed {0}: {1}'.format(obj, exc))

unreal.SystemLibrary.quit_editor()
