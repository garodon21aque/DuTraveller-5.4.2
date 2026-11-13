import json
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
report={"animations":{},"characters":{},"meshes":{}}
for path in ("/Game/Art/3D/Player/Animations/Anims/AnimBP_RPGAICharacter","/Game/Art/3D/Player/Animations/Anims/AnimBP_RPGCharacter"):
    report["animations"][path]=list(unreal.DTStructRepairLibrary.describe_anim_graph(path))
    unreal.DTStructRepairLibrary.dump_blueprint_graph(path)
for path in ("/Game/Blueprints/AI/BP_RPGAICharacter","/Game/Blueprints/AI/BP_AIRangedCharacter","/Game/Blueprints/AI/BP_RPGAIController"):
    bp=unreal.load_asset(path)
    cdo=unreal.get_default_object(bp.generated_class())
    row={}
    for prop in ("ai_controller_class","auto_possess_ai","attack_ability","starting_equipment"):
        try: row[prop]=str(cdo.get_editor_property(prop))
        except Exception: pass
    report["characters"][path]=row
for folder in ("/Game/Art/3D/Ai","/Game/Characters/Mannequin_UE4/Meshes"):
    for path in unreal.EditorAssetLibrary.list_assets(folder,True,False):
        asset=unreal.load_asset(path)
        if isinstance(asset,unreal.SkeletalMesh): report["meshes"][path]=str(asset.get_editor_property("skeleton"))
(root/"Saved/Automation/CombatEncounterAudit.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
unreal.log("COMBAT_ENCOUNTER_AUDIT "+json.dumps(report))
