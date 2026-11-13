"""Restores Blueprint-declared member variables that were dropped when their
owning Blueprint classes were regenerated as empty shells, or stripped at load
because their types were missing.

Serialized graphs reference these variables through Blueprint-scoped member
references (UBlueprint.NewVariables), so inherited native UPROPERTYs cannot
satisfy them - the variables must exist on the generated Blueprint assets.

Only variables with binary/compiler evidence are added, and the script refuses
to touch assets that already declare the variable.
"""

import unreal

_BASE = ('PinCategory="{cat}",PinSubCategory="{subcat}",PinSubCategoryObject={sub},'
         'PinSubCategoryMemberReference=(MemberParent=None,MemberName="",'
         'MemberGuid=00000000000000000000000000000000),'
         'PinValueType=(TerminalCategory="",TerminalSubCategory="",'
         'TerminalSubCategoryObject=None,bTerminalIsConst=False,'
         'bTerminalIsWeakPointer=False,bTerminalIsUObjectWrapper=False),'
         'ContainerType={container},bIsReference=False,bIsConst=False,'
         'bIsWeakPointer=False,bIsUObjectWrapper={wrapper},'
         'bSerializeAsSinglePrecisionFloat=False')


def _pin(category, class_object_path=None, container=None, sub_category=""):
    if class_object_path:
        sub = '"/Script/CoreUObject.Class\'{0}\'"'.format(class_object_path)
        wrapper = "True"
    else:
        sub = 'None'
        wrapper = "False"
    t = unreal.EdGraphPinType()
    text = "(" + _BASE.format(cat=category, subcat=sub_category, sub=sub,
                             container=container or "None",
                             wrapper=wrapper) + ")"
    t.import_text(text)
    return t


def _real_pin():
    return _pin("real", sub_category="float")


def _object_pin(class_object_path):
    return _pin("object", class_object_path)


def build_specs():
    return (
        ("/Game/Blueprints/Quest/BP_QuestGiver", (
            ("DataTable", _object_pin("/Script/Engine.DataTable")),
            ("QuestID", _pin("name")),
            ("CurrentObjectiveProgress", _pin("int")),
        )),
        ("/Game/Blueprints/Quest/BP_QuestLog", (
            ("CurrentQuests",
             _pin("object", "/Game/Blueprints/Quest/BP_QuestGiver.BP_QuestGiver_C",
                  "Array")),
        )),
        ("/Game/Blueprints/Player/BP_PlayerCharacter", (
            ("BP_QuestLog",
             _object_pin("/Game/Blueprints/Quest/BP_QuestLog.BP_QuestLog_C")),
        )),
        ("/Game/Blueprints/Player/BP_RPGCharacter", (
            ("BP_QuestLog",
             _object_pin("/Game/Blueprints/Quest/BP_QuestLog.BP_QuestLog_C")),
        )),
        ("/Game/UI/InventorySystem/WBP_PlayerStat", (
            ("Health", _real_pin()),
            ("Mana", _real_pin()),
            ("Level", _pin("int")),
        )),
        ("/Game/UI/World/WBP_ItemInteract", (
            ("Item", _object_pin("/Script/DuTraveller.ItemObject")),
        )),
        ("/Game/UI/InventorySystem/WBP_DropWidget", (
            ("InventoryWidget", _object_pin(
                "/Game/UI/InventorySystem/WBP_PlayerInventory.WBP_PlayerInventory_C")),
        )),
    )


def main():
    for object_path, members in build_specs():
        blueprint = unreal.load_asset(object_path)
        if not blueprint:
            raise RuntimeError("Unable to load {0}".format(object_path))
        try:
            existing = list(
                unreal.BlueprintEditorLibrary.list_member_variable_names(blueprint))
        except Exception:
            existing = []
        changed = False
        for var_name, pin_type in members:
            if var_name in existing:
                unreal.log("{0}: member {1} already exists".format(object_path, var_name))
                continue
            if unreal.BlueprintEditorLibrary.add_member_variable(
                    blueprint, var_name, pin_type):
                unreal.log("{0}: added member {1}".format(object_path, var_name))
                changed = True
            else:
                unreal.log_error("{0}: FAILED to add member {1}".format(
                    object_path, var_name))
        if changed:
            unreal.EditorAssetLibrary.save_loaded_asset(blueprint,
                                                        only_if_is_dirty=False)
            unreal.log("{0}: saved".format(object_path))


try:
    main()
except Exception as exc:  # noqa: BLE001
    unreal.log_error("Restore-BPMemberVariables failed: {0}".format(exc))
    raise
finally:
    unreal.SystemLibrary.quit_editor()
