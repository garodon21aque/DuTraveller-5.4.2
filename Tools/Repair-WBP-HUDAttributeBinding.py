"""Replace WBP_HUD's deleted component health lookup with replicated PlayerState GAS data.

This is intentionally scoped to the single stale GetComponentByClass call. The
native helper preserves its world-context and return links, then the existing
variable repair binds Health/MaxHealth to URPGAttributeSet. Saving is guarded by
a successful Blueprint compile.
"""
import unreal


ASSET = "/Game/UI/HUD/WBP_HUD.WBP_HUD"


try:
    repair = unreal.DTStructRepairLibrary
    replaced = repair.replace_blueprint_call_function(
        ASSET,
        "GetComponentByClass",
        "/Script/DuTraveller.RPG_AbilitySystemLibrary",
        "GetPlayerAttributeView",
    )
    if not replaced:
        raise RuntimeError("expected GetComponentByClass node was not replaced")
    repair.rebind_variable_ref_parents(ASSET)
    if not repair.force_compile_and_save(ASSET):
        raise RuntimeError("WBP_HUD did not compile; package was not saved")
    unreal.log("WBP_HUD_ATTRIBUTE_REPAIR_SUCCESS")
finally:
    unreal.SystemLibrary.quit_editor()
