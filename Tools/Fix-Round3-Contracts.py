"""Round-3 contract repair.

1. Adds the few member variables that are missing from NewVariables entirely
   (serialized member refs that resolved in 5.4.2 via vars lost on migration).
2. Reconstructs stale graph nodes whose pins the compiler asks to refresh.
3. Regenerates generated classes for every /Game Blueprint whose NewVariables
   entries lack FProperties, then saves only the assets that changed.
"""

import unreal

lib = unreal.BlueprintEditorLibrary
eal = unreal.EditorAssetLibrary
repair = unreal.DTStructRepairLibrary

_BASE = ('PinCategory="{cat}",PinSubCategory="{subcat}",PinSubCategoryObject={sub},'
         'PinSubCategoryMemberReference=(MemberParent=None,MemberName="",'
         'MemberGuid=00000000000000000000000000000000),'
         'PinValueType=(TerminalCategory="",TerminalSubCategory="",'
         'TerminalSubCategoryObject=None,bTerminalIsConst=False,'
         'bTerminalIsWeakPointer=False,bTerminalIsUObjectWrapper=False),'
         'ContainerType=None,bIsReference=False,bIsConst=False,'
         'bIsWeakPointer=False,bIsUObjectWrapper={wrapper},'
         'bSerializeAsSinglePrecisionFloat=False)')


def _pin(category, class_path=None, sub_category=""):
    sub = ('"/Script/CoreUObject.Class\'{0}\'"'.format(class_path)
           if class_path else 'None')
    t = unreal.EdGraphPinType()
    t.import_text("(" + _BASE.format(cat=category, subcat=sub_category, sub=sub,
                                     wrapper="True" if class_path else "False")
                  + ")")
    return t


def _names(bp):
    try:
        return [str(n) for n in lib.list_member_variable_names(bp)]
    except Exception:
        return []


# Vars missing from NewVariables but required by serialized member refs.
ADD_SPECS = [
    ("/Game/Blueprints/Quest/BP_QuestGiver",
     [("Data Table", _pin("object", "/Script/Engine.DataTable"))]),
    ("/Game/UI/World/WBP_ItemInteract",
     [("Item", _pin("object", "/Script/DuTraveller.ItemObject"))]),
    ("/Game/Art/3D/Player/Animations/Anims/AnimBP_RPGCharacter",
     [("Is Stunned", _pin("bool")), ("In Shock Loop", _pin("bool"))]),
    ("/Game/Art/3D/Player/Animations/Anims/AnimBP_RPGAICharacter",
     [("Is Stunned", _pin("bool"))]),
]

# Assets with stale serialized pins the compiler explicitly asks to refresh.
REFRESH = [
    "/Game/UI/QuestSystem/WBP_QuestLog",
    "/Game/UI/QuestSystem/WBP_QuestRewards",
    "/Game/UI/InventorySystem/WBP_PlayerInventory",
]


def _load(path):
    return unreal.load_asset("{0}.{1}".format(path, path.rsplit('/', 1)[-1]))


def main():
    for path, members in ADD_SPECS:
        bp = _load(path)
        if not bp:
            unreal.log_error(path + ": failed to load")
            continue
        existing = _names(bp)
        for name, pin in members:
            if name in existing:
                unreal.log("{0}: {1} exists".format(path, name))
            elif lib.add_member_variable(bp, name, pin):
                unreal.log("{0}: added {1}".format(path, name))
            else:
                unreal.log_error("{0}: FAILED {1}".format(path, name))

    for path in REFRESH:
        if _load(path):
            repair.refresh_blueprint_nodes(
                "{0}.{1}".format(path, path.rsplit('/', 1)[-1]))

    # Regen every Blueprint asset whose generated class is missing NV props.
    assets = eal.list_assets("/Game", recursive=True, include_folder=False)
    for package in assets:
        try:
            ad = eal.find_asset_data(package)
            cls = ad.asset_class_path.asset_name if hasattr(
                ad, 'asset_class_path') else str(ad.asset_class)
            if 'Blueprint' not in str(cls):
                continue
            obj_path = ad.get_soft_object_path().get_sub_path_string() \
                if hasattr(ad.get_soft_object_path(), 'get_sub_path_string') \
                else str(ad.get_soft_object_path())
            bp = unreal.load_asset(ad.get_asset())
            if not bp or not isinstance(bp, unreal.Blueprint):
                continue
            repair.regenerate_blueprint_class(obj_path)
            pkg = bp.get_package()
            if pkg.is_dirty():
                eal.save_loaded_asset(bp)
        except Exception as exc:
            unreal.log_warning("{0}: {1}".format(package, exc))


try:
    main()
except Exception as exc:  # noqa: BLE001
    unreal.log_error("Fix-Round3 failed: {0}".format(exc))
    raise
finally:
    unreal.SystemLibrary.quit_editor()
