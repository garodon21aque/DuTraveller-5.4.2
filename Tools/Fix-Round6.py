"""Round-6 contract repair — remaining seven failing Blueprints.

1. Fabricate the three skeleton assets the serialized AnimBlueprints
   reference but which were never committed (LFS evidence):
     /Game/Characters/Mannequin_UE4/Meshes/SK_Mannequin_Skeleton
     /Game/Art/AfricanAnimalsPack/LionAndLioness/Meshes/SK_Lion_LOD0_Skeleton
     /Game/Blueprints/AI/NPC/Animals/Wolf/Wolf/Meshes/SK_Wolf_Skeleton
   SK_Mannequin_Skeleton gets the canonical UE4 mannequin bone hierarchy
   (names verified against Elf_Mage_UE4.uasset); lion/wolf get a generic
   quadruped rig (original rigs unrecoverable).
2. Reassign TargetSkeleton on the four anim BPs (resaves had dropped the
   reference), re-add their missing member variables with exact serialized
   names, rebind null-parent member refs, regenerate and save.
3. WBP_QuestLog: refresh nodes so the Select rebuilds its EQuestTypes
   option pins (Main/Side/Bonus/Renown/Dungeon/Class).
4. BP_QuestLog: fix GetQuestActor result pins ('Array Element' ->
   BP_QuestGiver_C, 'Array_Element' -> int) and add the BP-level
   QueryCompletedQuest function WBP_QuestRewards serializes against.
5. WBP_QuestRewards: refresh the GetQuestActor call, then reconnect the
   dropped 'Array Element' -> IsAllObjectiveComplete.self link.
6. WBP_PlayerStat: retarget the dead cast to BP_RPGCharacter, retype the
   custom-event PC param to BP_RPGCharacter_C, and restore the serialized
   PC -> Initialize.self link.
"""

import unreal

r = unreal.DTStructRepairLibrary

MANNEQUIN_BONES = [
    "root|",
    "pelvis|root",
    "spine_01|pelvis", "spine_02|spine_01", "spine_03|spine_02",
    "clavicle_l|spine_03", "upperarm_l|clavicle_l", "lowerarm_l|upperarm_l",
    "hand_l|lowerarm_l",
    "index_01_l|hand_l", "index_02_l|index_01_l", "index_03_l|index_02_l",
    "middle_01_l|hand_l", "middle_02_l|middle_01_l", "middle_03_l|middle_02_l",
    "pinky_01_l|hand_l", "pinky_02_l|pinky_01_l", "pinky_03_l|pinky_02_l",
    "ring_01_l|hand_l", "ring_02_l|ring_01_l", "ring_03_l|ring_02_l",
    "thumb_01_l|hand_l", "thumb_02_l|thumb_01_l", "thumb_03_l|thumb_02_l",
    "lowerarm_twist_01_l|lowerarm_l", "upperarm_twist_01_l|upperarm_l",
    "neck_01|spine_03", "head|neck_01",
    "clavicle_r|spine_03", "upperarm_r|clavicle_r", "lowerarm_r|upperarm_r",
    "hand_r|lowerarm_r",
    "index_01_r|hand_r", "index_02_r|index_01_r", "index_03_r|index_02_r",
    "middle_01_r|hand_r", "middle_02_r|middle_01_r", "middle_03_r|middle_02_r",
    "pinky_01_r|hand_r", "pinky_02_r|pinky_01_r", "pinky_03_r|pinky_02_r",
    "ring_01_r|hand_r", "ring_02_r|ring_01_r", "ring_03_r|ring_02_r",
    "thumb_01_r|hand_r", "thumb_02_r|thumb_01_r", "thumb_03_r|thumb_02_r",
    "lowerarm_twist_01_r|lowerarm_r", "upperarm_twist_01_r|upperarm_r",
    "thigh_l|pelvis", "calf_l|thigh_l", "foot_l|calf_l", "ball_l|foot_l",
    "calf_twist_01_l|calf_l", "thigh_twist_01_l|thigh_l",
    "thigh_r|pelvis", "calf_r|thigh_r", "foot_r|calf_r", "ball_r|foot_r",
    "calf_twist_01_r|calf_r", "thigh_twist_01_r|thigh_r",
    "ik_foot_root|root", "ik_foot_l|ik_foot_root", "ik_foot_r|ik_foot_root",
    "ik_hand_root|root", "ik_hand_gun|ik_hand_root",
    "ik_hand_l|ik_hand_gun", "ik_hand_r|ik_hand_gun",
]

QUADRUPED_BONES = [
    "root|", "pelvis|root",
    "spine_01|pelvis", "spine_02|spine_01", "spine_03|spine_02",
    "neck_01|spine_03", "neck_02|neck_01", "head|neck_02", "jaw|head",
    "ear_l|head", "ear_r|head",
    "tail_01|pelvis", "tail_02|tail_01", "tail_03|tail_02",
    "clavicle_l|spine_03", "upperarm_l|clavicle_l", "lowerarm_l|upperarm_l",
    "hand_l|lowerarm_l",
    "clavicle_r|spine_03", "upperarm_r|clavicle_r", "lowerarm_r|upperarm_r",
    "hand_r|lowerarm_r",
    "thigh_l|pelvis", "calf_l|thigh_l", "foot_l|calf_l", "ball_l|foot_l",
    "thigh_r|pelvis", "calf_r|thigh_r", "foot_r|calf_r", "ball_r|foot_r",
]

ANIM_DIR = "/Game/Art/3D/Player/Animations/Anims"

ANIM_VARS = {
    "AnimBP_RPGCharacter": [
        (" BP Player Character", "object",
         "/Game/Blueprints/Player/BP_PlayerCharacter.BP_PlayerCharacter_C"),
        ("Direction", "real", ""),
        ("GroundSpeed", "real", ""),
        ("IsFalling", "bool", ""),
        ("Is Stunned", "bool", ""),
        ("In Shock Loop", "bool", ""),
        ("bIsBurning", "bool", ""),
    ],
    "AnimBP_RPGAICharacter": [
        ("AIPC", "object",
         "/Game/Blueprints/AI/BP_RPGAICharacter.BP_RPGAICharacter_C"),
        ("Direction", "real", ""),
        ("GroundSpeed", "real", ""),
        ("IsFalling", "bool", ""),
        ("Is Stunned", "bool", ""),
        ("bIsBurning", "bool", ""),
    ],
    "AnimBP_Lion": [
        ("Direction", "real", ""),
        ("GroundSpeed", "real", ""),
        ("IsFalling", "bool", ""),
        ("Is Stunned", "bool", ""),
        ("bIsBurning", "bool", ""),
    ],
    "AnimBP_Wolf": [
        ("Direction", "real", ""),
        ("GroundSpeed", "real", ""),
        ("IsFalling", "bool", ""),
        ("Is Stunned", "bool", ""),
        ("bIsBurning", "bool", ""),
    ],
}

ANIM_SKELETONS = {
    "AnimBP_RPGCharacter":
        "/Game/Characters/Mannequin_UE4/Meshes/SK_Mannequin_Skeleton"
        ".SK_Mannequin_Skeleton",
    "AnimBP_RPGAICharacter":
        "/Game/Characters/Mannequin_UE4/Meshes/SK_Mannequin_Skeleton"
        ".SK_Mannequin_Skeleton",
    "AnimBP_Lion":
        "/Game/Art/AfricanAnimalsPack/LionAndLioness/Meshes/"
        "SK_Lion_LOD0_Skeleton.SK_Lion_LOD0_Skeleton",
    "AnimBP_Wolf":
        "/Game/Blueprints/AI/NPC/Animals/Wolf/Wolf/Meshes/"
        "SK_Wolf_Skeleton.SK_Wolf_Skeleton",
}


def obj_path(dir_path, name):
    return "{0}.{1}".format(dir_path + "/" + name if not dir_path
                            .endswith(name) else dir_path, name)


def main():
    # --- 1. Fabricate missing skeleton assets ----------------------------
    for path, bones in [
        ("/Game/Characters/Mannequin_UE4/Meshes/SK_Mannequin_Skeleton"
         ".SK_Mannequin_Skeleton", MANNEQUIN_BONES),
        ("/Game/Art/AfricanAnimalsPack/LionAndLioness/Meshes/"
         "SK_Lion_LOD0_Skeleton.SK_Lion_LOD0_Skeleton", QUADRUPED_BONES),
        ("/Game/Blueprints/AI/NPC/Animals/Wolf/Wolf/Meshes/"
         "SK_Wolf_Skeleton.SK_Wolf_Skeleton", QUADRUPED_BONES),
    ]:
        unreal.log_warning("R6 skeleton: {0}".format(path))
        r.create_skeleton_asset(path, bones)
        r.save_asset_package(path)

    # --- 2. AnimBP skeletons + member vars + rebind + regen --------------
    for name, skeleton in ANIM_SKELETONS.items():
        path = "{0}/{1}.{1}".format(ANIM_DIR, name)
        unreal.log_warning("R6 animbp: {0}".format(path))
        r.assign_anim_skeleton(path, skeleton)
        for var_name, cat, sub in ANIM_VARS[name]:
            r.add_member_variable_exact(path, var_name, cat, sub)
        r.rebind_variable_ref_parents(path)
        r.regenerate_blueprint_class(path)
        r.force_compile_and_save(path)
        r.probe_var_state(path)

    # --- 3. WBP_QuestLog: rebuild stale Select enum pins -----------------
    ql = "/Game/UI/QuestSystem/WBP_QuestLog.WBP_QuestLog"
    unreal.log_warning("R6 refresh: {0}".format(ql))
    r.refresh_blueprint_nodes(ql)
    r.force_compile_and_save(ql)

    # --- 4. BP_QuestLog: fix GetQuestActor pins + QueryCompletedQuest ----
    bql = "/Game/Blueprints/Quest/BP_QuestLog.BP_QuestLog"
    unreal.log_warning("R6 pins: {0}".format(bql))
    r.set_user_pin_type(
        bql, "K2Node_FunctionResult_0", "Array Element", "object",
        "/Game/Blueprints/Quest/BP_QuestGiver.BP_QuestGiver_C")
    r.set_user_pin_type(
        bql, "K2Node_FunctionResult_0", "Array_Element", "int", "")
    r.create_bp_function(bql, "QueryCompletedQuest", [
        "in|QuestID|name|",
        "out|ReturnValue|bool|",
        "out|TurnedIn|bool|",
    ])
    r.force_compile_and_save(bql)

    # --- 5. WBP_QuestRewards: refresh + reconnect dropped link -----------
    qr = "/Game/UI/QuestSystem/WBP_QuestRewards.WBP_QuestRewards"
    unreal.log_warning("R6 questrewards: {0}".format(qr))
    r.refresh_blueprint_nodes(qr)
    r.connect_blueprint_pins(
        qr, "K2Node_CallFunction_9", "Array Element",
        "K2Node_CallFunction_18", "self")
    r.force_compile_and_save(qr)

    # --- 6. WBP_PlayerStat: cast target + PC param + self link -----------
    ps = "/Game/UI/InventorySystem/WBP_PlayerStat.WBP_PlayerStat"
    unreal.log_warning("R6 playerstat: {0}".format(ps))
    r.set_cast_target_class(
        ps, "K2Node_DynamicCast_0",
        "/Game/Blueprints/Player/BP_RPGCharacter.BP_RPGCharacter_C")
    r.set_user_pin_type(
        ps, "K2Node_CustomEvent_0", "PC", "object",
        "/Game/Blueprints/Player/BP_RPGCharacter.BP_RPGCharacter_C")
    r.connect_blueprint_pins(
        ps, "K2Node_CustomEvent_0", "PC", "K2Node_CallFunction_0", "self")
    r.refresh_blueprint_nodes(ps)
    r.force_compile_and_save(ps)

    unreal.log_warning("R6 done")


try:
    main()
except Exception as exc:  # noqa: BLE001
    unreal.log_error("Fix-Round6 failed: {0}".format(exc))
    raise
finally:
    unreal.SystemLibrary.quit_editor()
