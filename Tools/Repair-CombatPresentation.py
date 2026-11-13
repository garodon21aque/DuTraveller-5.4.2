"""Repair retained combat presentation assets; leave traversal on DefaultSlot."""
import json,shutil,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir()); report={"passed":False,"assets":[],"errors":[]}
repair=unreal.DTStructRepairLibrary
def backup(path):
    source=root/("Content/"+path.removeprefix("/Game/").split(".")[0]+".uasset")
    target=root/"Saved/Backups/CombatPresentation"/source.relative_to(root)
    if source.exists() and not target.exists(): target.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(source,target)
try:
    for path in ("/Game/Blueprints/Animations/ABP_SandboxCharacter","/Game/Art/3D/Player/Animations/Anims/AnimBP_RPGAICharacter"):
        backup(path)
        before=unreal.load_asset(path)
        backup(before.get_editor_property("target_skeleton").get_path_name())
        if not repair.repair_combat_upper_body_blend(path): raise RuntimeError("Upper-body blend repair failed "+path)
        if not repair.force_compile_and_save(path): raise RuntimeError("AnimBP compile failed "+path)
        asset=unreal.load_asset(path); skeleton=asset.get_editor_property("target_skeleton")
        unreal.EditorAssetLibrary.save_loaded_asset(skeleton,False)
        report["assets"].append(path)
    montages=set()
    for path in ("/Game/Blueprints/Characters/BP_PlayerCharacter","/Game/Blueprints/AI/BP_CombatTest_Enemy"):
        cdo=unreal.get_default_object(unreal.load_asset(path).generated_class())
        router=cdo.get_editor_property("animation_router")
        entries=list(router.get_editor_property("combat_animations").values())+[router.get_editor_property("hit_reaction")]
        for entry in entries:
            montage=entry.get_editor_property("montage")
            if montage: montages.add(montage.get_path_name().split(".")[0])
    for path in sorted(montages):
        backup(path)
        if not repair.set_montage_slot_name(path,"Upperbody"): raise RuntimeError("Montage slot repair failed "+path)
        report["assets"].append(path)
    path="/Game/Blueprints/Gameplay/Projectiles/BP_Projectile_FireBolt"
    backup(path)
    asset=unreal.load_asset(path); unreal.BlueprintEditorLibrary.compile_blueprint(asset)
    cdo=unreal.get_default_object(asset.generated_class())
    cdo.set_editor_property("impact_fx",unreal.load_asset("/Game/Art/Niagara/Explode/Fire_Burst"))
    cdo.set_editor_property("impact_fx_lifetime",1.5)
    unreal.EditorAssetLibrary.save_loaded_asset(asset,False); report["assets"].append(path)
    report["passed"]=True
    path="/Game/Blueprints/AI/BP_CombatTest_Enemy"
    backup(path); asset=unreal.load_asset(path); unreal.BlueprintEditorLibrary.compile_blueprint(asset)
    cdo=unreal.get_default_object(asset.generated_class()); mesh=cdo.get_editor_property("mesh")
    report["enemy_rotation_before"]=str(mesh.get_editor_property("relative_rotation"))
    mesh.set_editor_property("relative_rotation",unreal.Rotator(pitch=0,yaw=-90,roll=0))
    cdo.set_editor_property("use_controller_rotation_pitch",False); cdo.set_editor_property("use_controller_rotation_roll",False)
    cdo.set_editor_property("enemy_class","Combat Test Enemy")
    unreal.EditorAssetLibrary.save_loaded_asset(asset,False); report["assets"].append(path)
    report["enemy_rotation_after"]=str(mesh.get_editor_property("relative_rotation"))
except Exception:
    report["passed"]=False
    report["errors"].append(traceback.format_exc()); unreal.log_error(report["errors"][-1])
finally:
    (root/"Saved/Automation/CombatPresentationRepair.json").write_text(json.dumps(report,indent=2))
    unreal.log("COMBAT_PRESENTATION_REPAIR "+json.dumps(report))
