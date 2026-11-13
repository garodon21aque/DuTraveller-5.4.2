import unreal
r = unreal.DTStructRepairLibrary
r.dump_blueprint_graph('/Game/UI/QuestSystem/WBP_QuestLog.WBP_QuestLog')
r.dump_blueprint_graph('/Game/UI/World/WBP_ItemInteract.WBP_ItemInteract')
r.dump_blueprint_graph('/Game/UI/InventorySystem/WBP_PlayerInventory.WBP_PlayerInventory')
r.probe_var_state('/Game/Blueprints/Quest/BP_QuestLog.BP_QuestLog')
r.probe_var_state('/Game/UI/World/WBP_ItemInteract.WBP_ItemInteract')
