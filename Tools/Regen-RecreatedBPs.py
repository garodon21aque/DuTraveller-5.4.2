import unreal
repair = unreal.DTStructRepairLibrary
eal = unreal.EditorAssetLibrary
lib = unreal.BlueprintEditorLibrary

TARGETS = [
    "/Game/Blueprints/Quest/BP_QuestGiver",
    "/Game/Blueprints/Quest/BP_QuestLog",
    "/Game/Blueprints/Player/BP_PlayerCharacter",
    "/Game/Blueprints/Player/BP_RPGCharacter",
    "/Game/Blueprints/Player/BP_RPGPlayerController",
    "/Game/Blueprints/Framework/BP_RPGPlayerState",
    "/Game/Blueprints/AI/BP_RPGAICharacter",
    "/Game/Blueprints/AI/BP_AIRangedCharacter",
    "/Game/Blueprints/AI/BP_RPGAIController",
    "/Game/Blueprints/AI/NPC/BP_NPC_Trainer",
    "/Game/Blueprints/AI/NPC/BP_AIRPGActionComponent",
    "/Game/Blueprints/Actions/Melee/BP_Action_Swing",
    "/Game/Blueprints/Actions/Melee/BP_RPGAction_MeleeAttack",
    "/Game/Blueprints/Actors/Interactables/Items/BP_MasterPickupItem",
    "/Game/Blueprints/Actors/Interactables/Items/BP_Pickup",
    "/Game/Blueprints/InventorySystem/Items/MoneyItem/BP_BronzeItem",
    "/Game/Blueprints/InventorySystem/Items/MoneyItem/BP_GoldItem",
    "/Game/Blueprints/Buildings/Village/BP_Farm",
    "/Game/Blueprints/Buildings/Village/BP_Tavern",
    "/Game/Blueprints/Buildings/Village/BP_TownHall",
    "/Game/Blueprints/Notify/BP_DealDamageNotify",
]

for path in TARGETS:
    obj_path = "{0}.{1}".format(path, path.rsplit('/',1)[-1])
    bp = unreal.load_asset(obj_path)
    if not bp:
        unreal.log_error("missing: " + obj_path)
        continue
    repair.regenerate_blueprint_class(obj_path)
    eal.save_loaded_asset(bp, only_if_is_dirty=False)
    unreal.log("regen+saved: " + path)

unreal.SystemLibrary.quit_editor()
