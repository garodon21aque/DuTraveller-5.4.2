import unreal
repair = unreal.DTStructRepairLibrary
paths = [
    "/Game/DTIntegration/Animation/Combat/DT_Standing_1H_Cast_Spell_01_UEFN",
    "/Game/DTIntegration/Animation/Combat/DT_Standing_Melee_Attack_Horizontal_UEFN",
    "/Game/DTIntegration/Animation/Combat/DT_Standing_Melee_Attack_360_High_UEFN",
    "/Game/DTIntegration/Animation/Combat/DT_AMAI_ReactHitFont_UEFN",
    "/Game/DTIntegration/Animation/Combat/DT_Sword_And_Shield_Slash_mixamo_com_Montage_UEFN",
]
for p in paths:
    ok = repair.set_montage_slot_name(p, "DefaultSlot")
    unreal.log("SLOT %s -> %s" % (p, ok))
    for line in repair.describe_montage_notifies(p):
        if "slottrack" in line or "montage=" in line:
            unreal.log("  " + line)
unreal.SystemLibrary.quit_editor()
