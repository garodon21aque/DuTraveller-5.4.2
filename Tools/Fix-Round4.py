import unreal

lib = unreal.DTStructRepairLibrary
elib = unreal.EditorAssetLibrary

ENUM_PATH = "/Game/Blueprints/Quest/EQuestTypes.EQuestTypes"
QUESTLOG_BP = "/Game/Blueprints/Quest/BP_QuestLog.BP_QuestLog"

# 1. Correct enum entry order (historical: Main,Side,Bonus,Renown,Dungeon,Class).
lib.set_user_defined_enum_entry_names(
    ENUM_PATH, ["Main", "Side", "Bonus", "Renown", "Dungeon", "Class"])

# 2. Ensure GetQuestActor exists on BP_QuestLog (idempotent), then
#    compile+save so the UFunction lands on BP_QuestLog_C.
pins = [
    "in|QuestID|name|None",
    "out|Array Element|object|/Game/Blueprints/Quest/BP_QuestGiver",
    "out|Array_Element|object|/Game/Blueprints/Quest/BP_QuestGiver",
    "out|ReturnValue|object|/Script/DuTraveller.RPGQuest",
]
ok = lib.create_bp_function(QUESTLOG_BP, "GetQuestActor", pins)
unreal.log("R4 create_func=%d" % (1 if ok else 0))
lib.force_compile_and_save(QUESTLOG_BP)

# 3. Rebind external member refs with unresolved parents on WBP_ItemInteract.
lib.rebind_variable_ref_parents(
    "/Game/UI/World/WBP_ItemInteract.WBP_ItemInteract")

# 4. Connect the dropped Array link in WBP_PlayerInventory.
lib.connect_blueprint_pins(
    "/Game/UI/InventorySystem/WBP_PlayerInventory.WBP_PlayerInventory",
    "K2Node_MakeArray_2", "Array",
    "K2Node_GetArrayItem_0", "Array")

# 5. Refresh (link-preserving + orphan removal) and save every stale-pin asset.
for path in [
    "/Game/UI/QuestSystem/WBP_QuestLog.WBP_QuestLog",
    "/Game/UI/QuestSystem/WBP_QuestRewards.WBP_QuestRewards",
    "/Game/UI/World/WBP_ItemInteract.WBP_ItemInteract",
    "/Game/UI/InventorySystem/WBP_PlayerInventory.WBP_PlayerInventory",
    "/Game/Art/3D/Player/Animations/Anims/AnimBP_RPGCharacter.AnimBP_RPGCharacter",
    "/Game/Art/3D/Player/Animations/Anims/AnimBP_RPGAICharacter.AnimBP_RPGAICharacter",
    "/Game/Art/3D/Player/Animations/Anims/AnimBP_Lion.AnimBP_Lion",
    "/Game/Art/3D/Player/Animations/Anims/AnimBP_Wolf.AnimBP_Wolf",
]:
    lib.refresh_blueprint_nodes(path)
    lib.save_asset_package(path)

unreal.log("R4 done")
