"""Round-2 contract repair: add remaining stripped Blueprint variables, fix the
CurrentObjectiveProgress map type, expose AttachedActor on spawned widgets, and
compile+save every touched asset so generated classes serialize the new
FProperties (var-get member references resolve against the generated class).
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


def _real():
    return _pin("real", sub_category="float")


def _obj(path):
    return _pin("object", path)


lib = unreal.BlueprintEditorLibrary
eal = unreal.EditorAssetLibrary

# Vars to add: asset -> [(name, pin, expose_on_spawn)]
ADD_SPECS = [
    ("/Game/UI/HUD/WBP_HUD", [
        ("Health", _real(), False),
        ("MaxHealth", _real(), False),
    ]),
    ("/Game/Blueprints/Player/BP_RPGPlayerController", [
        ("InventoryWidget",
         _obj("/Game/UI/InventorySystem/WBP_PlayerInventory.WBP_PlayerInventory_C"),
         False),
    ]),
    ("/Game/UI/World/WBP_XPPopUp", [("AttachedActor", _obj("/Script/Engine.Actor"), True)]),
    ("/Game/UI/World/WBP_LevelUpNotification", [("AttachedActor", _obj("/Script/Engine.Actor"), True)]),
    ("/Game/UI/QuestSystem/WBP_Objectives", [("AttachedActor", _obj("/Script/Engine.Actor"), True)]),
    ("/Game/UI/QuestSystem/WBP_QuestLog_QuestEntry", [("AttachedActor", _obj("/Script/Engine.Actor"), True)]),
    ("/Game/UI/InventorySystem/WBP_ItemObject", [("AttachedActor", _obj("/Script/Engine.Actor"), True)]),
    ("/Game/UI/InventorySystem/WBP_VendorItemObject", [("AttachedActor", _obj("/Script/Engine.Actor"), True)]),
    ("/Game/UI/HUD/SpellGlobes/WBP_SpellGlobe", [("AttachedActor", _obj("/Script/Engine.Actor"), True)]),
]

# Assets that received vars in earlier passes and must now be compiled+saved.
RECOMPILE = [
    "/Game/Blueprints/Quest/BP_QuestGiver",
    "/Game/Blueprints/Quest/BP_QuestLog",
    "/Game/Blueprints/Player/BP_PlayerCharacter",
    "/Game/Blueprints/Player/BP_RPGCharacter",
    "/Game/UI/InventorySystem/WBP_PlayerStat",
    "/Game/UI/World/WBP_ItemInteract",
    "/Game/UI/InventorySystem/WBP_DropWidget",
    "/Game/UI/World/WPB_TargetEnemy",
    "/Game/UI/InventorySystem/WBP_NPCVendorShop",
    "/Game/UI/HUD/WBP_HUD",
    "/Game/Blueprints/Player/BP_RPGPlayerController",
]


def _names(bp):
    try:
        return [str(n) for n in lib.list_member_variable_names(bp)]
    except Exception:
        return []


def _compile_save(bp, path):
    try:
        lib.compile_blueprint(bp)
    except Exception as exc:
        unreal.log_warning("{0}: compile failed: {1}".format(path, exc))
    eal.save_loaded_asset(bp, only_if_is_dirty=False)
    unreal.log("{0}: compiled+saved".format(path))


def main():
    touched = []
    for path, members in ADD_SPECS:
        bp = unreal.load_asset(path)
        if not bp:
            unreal.log_error("{0}: failed to load".format(path))
            continue
        existing = _names(bp)
        for name, pin, expose in members:
            if name in existing:
                unreal.log("{0}: {1} exists".format(path, name))
            elif lib.add_member_variable(bp, name, pin):
                unreal.log("{0}: added {1}".format(path, name))
                existing.append(name)
            else:
                unreal.log_error("{0}: FAILED to add {1}".format(path, name))
                continue
            if expose:
                try:
                    lib.set_blueprint_variable_expose_on_spawn(bp, name, True)
                except Exception as exc:
                    unreal.log_warning("{0}: expose failed for {1}: {2}".format(
                        path, name, exc))
        touched.append((path, bp))

    # Retype CurrentObjectiveProgress -> TMap<FName,int32>.
    qg = unreal.load_asset("/Game/Blueprints/Quest/BP_QuestGiver")
    if qg:
        map_pin = _pin("name", container="Map", terminal_category="int")
        try:
            lib.change_member_variable_type(qg, "CurrentObjectiveProgress", map_pin)
            unreal.log("BP_QuestGiver: CurrentObjectiveProgress retyped to map")
        except Exception as exc:
            unreal.log_warning("retype failed: {0}".format(exc))
        touched.append(("/Game/Blueprints/Quest/BP_QuestGiver", qg))

    # Compile+save every asset that received variables in any pass.
    for path in RECOMPILE:
        if any(t[0] == path for t in touched):
            continue
        bp = unreal.load_asset(path)
        if bp:
            touched.append((path, bp))
        else:
            unreal.log_error("{0}: failed to load for recompile".format(path))

    for path, bp in touched:
        _compile_save(bp, path)


try:
    main()
except Exception as exc:  # noqa: BLE001
    unreal.log_error("Fix-Round2 failed: {0}".format(exc))
    raise
finally:
    unreal.SystemLibrary.quit_editor()
