import unreal

for p in ("/Game/UI/InventorySystem/WBP_PlayerStat",
          "/Game/UI/World/WBP_ItemInteract",
          "/Game/UI/InventorySystem/WBP_DropWidget",
          "/Game/UI/HUD/WBP_HUD"):
    bp = unreal.load_asset(p)
    names = unreal.BlueprintEditorLibrary.list_member_variable_names(bp)
    unreal.log("{0}: {1}".format(p, [n for n in names if "/" not in n]))
