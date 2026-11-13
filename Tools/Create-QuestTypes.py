"""Recreates the missing quest user-defined structs/enums at their original
/Game/Blueprints/Quest paths. Field VarNames are the exact historical names
recovered from referencing widget assets (format Friendly_<id>_<guid32>), so
serialized member references in the graphs resolve again.
"""

import unreal

QUEST_DIR = "/Game/Blueprints/Quest"
FOBJ = QUEST_DIR + "/FObjectiveDetails"
FSTAGE = QUEST_DIR + "/FStageDetails"
FQUEST = QUEST_DIR + "/FQuestDetails"
EQUEST = QUEST_DIR + "/EQuestTypes"
EOBJ = QUEST_DIR + "/EQObjctiveTypes"

S = unreal.DTStructRepairLibrary.create_user_defined_struct_asset
E = unreal.DTStructRepairLibrary.create_user_defined_enum_asset


def save(path):
    asset = unreal.load_asset(path)
    if asset:
        unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
        unreal.log("saved {0}".format(path))


def main():
    # Enums first (struct fields reference them). Entries recovered from
    # Select-node option pins in WBP_QuestLog.
    unreal.log("EQuestTypes: {0}".format(
        E(EQUEST + ".EQuestTypes",
          ["Main", "Side", "Renown", "Dungeon", "Class", "Bonus"])))
    unreal.log("EQObjctiveTypes: {0}".format(
        E(EOBJ + ".EQObjctiveTypes",
          ["Interact", "Collect", "Kill", "Talk", "Location"])))

    # SetEnums must run after creation: SetEnumeratorDisplayName only writes
    # the DisplayNameMap, leaving entries named EQuestTypes::NewEnumeratorN,
    # while serialized Select/literal pins look up the real entry names.
    RN = unreal.DTStructRepairLibrary.set_user_defined_enum_entry_names
    unreal.log("EQuestTypes names: {0}".format(
        RN(EQUEST + ".EQuestTypes",
           ["Main", "Side", "Renown", "Dungeon", "Class", "Bonus"])))
    unreal.log("EQObjctiveTypes names: {0}".format(
        RN(EOBJ + ".EQObjctiveTypes",
           ["Interact", "Collect", "Kill", "Talk", "Location"])))

    # Spec format: Name|Category|SubPath|Container|VarName — SubPath must be
    # "None" (never empty) for primitives, otherwise ParseIntoArray's cull-empty
    # collapses the field and shifts VarName out of position.
    # FObjectiveDetails — fields ordered by recovered VarName index.
    unreal.log("FObjectiveDetails: {0}".format(S(FOBJ + ".FObjectiveDetails", [
        "ObjectiveID|name|None|None|ObjectiveID_8_3D7F256C4B83FE738FCAF49DB8BAD2A7",
        "ObjectiveName|text|None|None|ObjectiveName_9_19DAA0F8426693C668FC1198E8602CDC",
        "ObjectiveDescription|text|None|None|ObjectiveDescription_10_29D5AF864DB45F68FC5ABA918E6BBBCD",
        "TrackingDescription|text|None|None|TrackingDescription_12_52EED7A3472E5664FDB26EA1283F7484",
        "Quantity|int|None|None|Quantity_13_946EF48E4A2CE666D895E59340054D6E",
        "isOptional|bool|None|None|isOptional_16_FE33B4A348288CEA9FA50A938B77FC34",
        "BonusXP|int|None|None|BonusXP_19_091209D84637664093641BB9D83CC049",
        "BonusRenown|int|None|None|BonusRenown_24_82EDBCDA4AB1910326B1D1A169108B31",
        "ObjectiveType|byte|" + EOBJ + ".EQObjctiveTypes|None|"
            "ObjectiveType_26_DED500EF48938225045C6BB93DA14E0A",
    ])))

    # FStageDetails
    unreal.log("FStageDetails: {0}".format(S(FSTAGE + ".FStageDetails", [
        "StageName|text|None|None|StageName_4_B9D16E574CE572C0AE73779C639DA95E",
        "StageDescription|text|None|None|StageDescription_6_03D28C1A47E3C4195EC0188C46F0DACD",
        "LogDescription|text|None|None|LogDescription_8_37E13ED8408002DF0CB1E88A15159FFF",
        "XPReward|int|None|None|XPReward_10_2EA01F044DE38AD0E722F0BD6BBD22EA",
        "CurrencyRewards|int|None|None|CurrencyRewards_20_97CAE10B411C1178E95EFD925CE4A8B8",
        "Renown|int|None|None|Renown_25_CC5260CA4BC1EFF6F530959A9BEE58FB",
        "Objectives|struct|" + FOBJ + ".FObjectiveDetails|Array|"
            "Objectives_27_604C66DE45402E70A1E3DD9D985D6F20",
        "ItemRewards|class|/Script/DuTraveller.ItemObject|Array|"
            "ItemRewards_44_ABB625DD4B808041897975A403F96508",
    ])))

    # FQuestDetails
    unreal.log("FQuestDetails: {0}".format(S(FQUEST + ".FQuestDetails", [
        "QuestName|text|None|None|QuestName_5_29411FF641A0BF4D7D2534AA5DC33C3B",
        "LogDescription|text|None|None|LogDescription_8_37E13ED8408002DF0CB1E88A15159FFF",
        "Stages|struct|" + FSTAGE + ".FStageDetails|Array|"
            "Stages_18_561DB2654DDA7A02517F4391AF0B9F97",
        "QuestTypes|byte|" + EQUEST + ".EQuestTypes|None|"
            "QuestTypes_19_B732A48F4C75B06B145FFA95960210AD",
        "CurrencyRewards|int|None|None|CurrencyRewards_20_97CAE10B411C1178E95EFD925CE4A8B8",
        "ItemRewards|class|/Script/DuTraveller.ItemObject|Array|"
            "ItemRewards_44_ABB625DD4B808041897975A403F96508",
        "XPReward|int|None|None|XPReward_10_2EA01F044DE38AD0E722F0BD6BBD22EA",
    ])))

    for p in (EQUEST, EOBJ, FOBJ, FSTAGE, FQUEST):
        save(p)


try:
    main()
except Exception as exc:  # noqa: BLE001
    unreal.log_error("Create-QuestTypes failed: {0}".format(exc))
    raise
finally:
    unreal.SystemLibrary.quit_editor()
