"""Remove the obsolete ComponentClass orphan pin left by the targeted HUD call replacement."""
import unreal

ASSET = "/Game/UI/HUD/WBP_HUD.WBP_HUD"
try:
    repair = unreal.DTStructRepairLibrary
    repair.refresh_blueprint_nodes(ASSET)
    repair.rebind_variable_ref_parents(ASSET)
    if not repair.force_compile_and_save(ASSET):
        raise RuntimeError("WBP_HUD did not compile; package was not saved")
    unreal.log("WBP_HUD_FINALIZE_SUCCESS")
finally:
    unreal.SystemLibrary.quit_editor()
