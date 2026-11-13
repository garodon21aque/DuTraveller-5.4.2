import unreal
for path,props in [('/Game/UI/World/WBP_ItemInteract',['Item']),
                   ('/Game/UI/InventorySystem/WBP_PlayerStat',['Health','Mana','Level']),
                   ('/Game/Blueprints/Quest/BP_QuestGiver',['DataTable','Data Table','QuestID','CurrentObjectiveProgress']),
                   ('/Game/UI/World/WPB_TargetEnemy',['EnemyActor','Enemy Actor']),
                   ('/Game/UI/HUD/WBP_HUD',['Health','MaxHealth']),
                   ('/Game/Blueprints/Player/BP_RPGPlayerController',['InventoryWidget'])]:
    bp=unreal.load_asset(path)
    gc=bp.generated_class()
    cdo=gc.get_default_object()
    for p in props:
        try:
            v=cdo.get_editor_property(p)
            print(path,'|',p,'= OK',type(v))
        except Exception as e:
            print(path,'|',p,'= MISSING')
import sys
