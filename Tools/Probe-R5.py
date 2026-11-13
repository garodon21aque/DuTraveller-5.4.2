import unreal
r = unreal.DTStructRepairLibrary

for obj in [
    '/Game/Blueprints/Quest/BP_QuestLog.BP_QuestLog',
    '/Game/UI/QuestSystem/WBP_QuestRewards.WBP_QuestRewards',
    '/Game/UI/InventorySystem/WBP_PlayerStat.WBP_PlayerStat',
    '/Game/Art/3D/Player/Animations/Anims/AnimBP_RPGCharacter.AnimBP_RPGCharacter',
    '/Game/Art/3D/Player/Animations/Anims/AnimBP_RPGAICharacter.AnimBP_RPGAICharacter',
]:
    try:
        unreal.log_warning('PROBE ==== %s' % obj)
        r.dump_blueprint_graph(obj)
        r.probe_var_state(obj)
    except Exception as exc:
        unreal.log_error('probe failed %s: %s' % (obj, exc))

unreal.SystemLibrary.quit_editor()
