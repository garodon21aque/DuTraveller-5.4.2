"""Round-3 contract repair.

1. Rename BP_QuestGiver 'DataTable' -> 'Data Table' (serialized member refs use
   the space name).
2. Re-add 'Item' (ItemObject) on WBP_ItemInteract and retype ' BP Pickup' to
   BP_MasterPickupItem (pin evidence: 'BP Master Pickup Item Object Reference').
3. Reconstruct stale nodes (Select enum pins, GetQuestActor out pin, anim casts)
   now that the enum entry names and signatures match the serialized contract.
4. Regenerate classes whose NewVariables changed; save only touched assets.
"""

import unreal

_BASE = ('PinCategory="{cat}",PinSubCategory="{subcat}",PinSubCategoryObject={sub},'
         'PinSubCategoryMemberReference=(MemberParent=None,MemberName="",'
         'MemberGuid=00000000000000000000000000000000),'
         'PinValueType=(TerminalCategory="{tcat}",TerminalSubCategory="",'
         'TerminalSubCategoryObject=None,bTerminalIsConst=False,'
         'bTerminalIsWeakPointer=False,bTerminalIsUObjectWrapper=False),'
         'ContainerType={container},bIsReference=False,bIsConst=False,'
         'bIsWeakPointer=False,bIsUObjectWrapper={wrapper},'
         'bSerializeAsSinglePrecisionFloat=False)')


def _pin(category, class_object_path=None, container="None", sub_category="",
         terminal_category=""):
    if class_object_path:
        sub = '"/Script/CoreUObject.Class\'{0}\'"'.format(class_object_path)
        wrapper = "True"
    else:
        sub = 'None'
        wrapper = "False"
    t = unreal.EdGraphPinType()
    text = "(" + _BASE.format(cat=category, subcat=sub_category, sub=sub,
                             tcat=terminal_category, container=container,
                             wrapper=wrapper) + ")"
    t.import_text(text)
    return t


def _obj(path):
    return _pin("object", path)


lib = unreal.BlueprintEditorLibrary
eal = unreal.EditorAssetLibrary
repair = unreal.DTStructRepairLibrary

# Assets whose stale pins need ReconstructNode now that contracts resolve.
REFRESH = [
    "/Game/UI/QuestSystem/WBP_QuestLog",
    "/Game/UI/QuestSystem/WBP_QuestRewards",
    "/Game/UI/QuestSystem/WBP_QuestLog_QuestEntry",
    "/Game/UI/World/WBP_ItemInteract",
    "/Game/UI/World/WPB_TargetEnemy",
    "/Game/UI/InventorySystem/WBP_PlayerInventory",
    "/Game/Art/3D/Player/Animations/Anims/AnimBP_RPGCharacter",
    "/Game/Art/3D/Player/Animations/Anims/AnimBP_RPGAICharacter",
]

saved = []


def _save(bp, path):
    eal.save_loaded_asset(bp, only_if_is_dirty=False)
    saved.append(path)


def main():
    # --- 1. BP_QuestGiver: DataTable -> 'Data Table' ----------------------
    qg = unreal.load_asset("/Game/Blueprints/Quest/BP_QuestGiver")
    names = [str(n) for n in lib.list_member_variable_names(qg)]
    if "Data Table" in names:
        unreal.log("BP_QuestGiver: 'Data Table' already present")
    elif "DataTable" in names:
        try:
            lib.rename_member_variable(qg, "DataTable", "Data Table")
            unreal.log("BP_QuestGiver: renamed DataTable -> 'Data Table'")
        except Exception as exc:  # noqa: BLE001
            unreal.log_warning("rename failed, adding new var: {0}".format(exc))
            lib.add_member_variable(qg, "Data Table",
                                    _obj("/Script/Engine.DataTable"))
    else:
        lib.add_member_variable(qg, "Data Table",
                                _obj("/Script/Engine.DataTable"))
        unreal.log("BP_QuestGiver: added 'Data Table'")
    repair.regenerate_blueprint_class(
        "/Game/Blueprints/Quest/BP_QuestGiver.BP_QuestGiver")
    _save(qg, "BP_QuestGiver")

    # --- 2. WBP_ItemInteract: Item var + BP Pickup retype -----------------
    ii = unreal.load_asset("/Game/UI/World/WBP_ItemInteract")
    names = [str(n) for n in lib.list_member_variable_names(ii)]
    if "Item" not in names:
        lib.add_member_variable(ii, "Item",
                                _obj("/Script/DuTraveller.ItemObject"))
        unreal.log("WBP_ItemInteract: added Item")
    for vname in (" BP Pickup", "BP_Pickup"):
        if vname in names:
            try:
                lib.change_member_variable_type(
                    ii, vname,
                    _obj("/Game/Blueprints/Actors/Interactables/Items/"
                         "BP_MasterPickupItem.BP_MasterPickupItem_C"))
                unreal.log("WBP_ItemInteract: retyped '{0}'".format(vname))
            except Exception as exc:  # noqa: BLE001
                unreal.log_warning("retype {0} failed: {1}".format(vname, exc))
    repair.refresh_blueprint_nodes(
        "/Game/UI/World/WBP_ItemInteract.WBP_ItemInteract")
    repair.regenerate_blueprint_class(
        "/Game/UI/World/WBP_ItemInteract.WBP_ItemInteract")
    _save(ii, "WBP_ItemInteract")

    # --- 3. Node refresh on stale-pin assets -------------------------------
    for path in REFRESH:
        obj = path + "." + path.rsplit("/", 1)[-1]
        if repair.refresh_blueprint_nodes(obj):
            bp = unreal.load_asset(path)
            if bp:
                _save(bp, path)

    unreal.log("Round-3 complete, saved {0} assets".format(len(saved)))


try:
    main()
except Exception as exc:  # noqa: BLE001
    unreal.log_error("Fix-Round3 failed: {0}".format(exc))
    raise
finally:
    unreal.SystemLibrary.quit_editor()
