import unreal

paths = [
    "/Game/UI/InventorySystem/WBP_PlayerStat",
    "/Game/UI/World/WBP_ItemInteract",
    "/Game/UI/InventorySystem/WBP_NPCVendorShop",
    "/Game/UI/InventorySystem/WBP_DropWidget",
    "/Game/UI/World/WPB_TargetEnemy",
]
for p in paths:
    unreal.log("=== {0}".format(p))
    try:
        bp = unreal.load_asset(p)
        unreal.log("  loaded: {0}".format(bool(bp)))
        if bp:
            names = unreal.BlueprintEditorLibrary.list_member_variable_names(bp)
            unreal.log("  vars: {0}".format(names))
    except Exception as e:
        unreal.log("  EXC: {0}".format(e))
