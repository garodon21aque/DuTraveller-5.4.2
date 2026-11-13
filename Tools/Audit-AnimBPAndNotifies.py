"""Dump anim-graph node classes of the live AnimBP, notify BPs, and AI anim assets."""
import json
from pathlib import Path
import unreal

report = {"anim_graph_nodes": {}, "montage_notify_detail": {}, "ai_anims": [],
          "niagara": [], "deal_damage_notify": None, "errors": []}

# 1) Anim graph node inventory for the live + legacy anim BPs
for path in ["/Game/Blueprints/Animations/ABP_SandboxCharacter",
             "/Game/Art/3D/Player/Animations/Anims/AnimBP_RPGCharacter",
             "/Game/Art/3D/Player/Animations/Anims/AnimBP_RPGAICharacter"]:
    try:
        bp = unreal.load_asset(path)
        if not bp:
            report["anim_graph_nodes"][path] = {"missing": True}
            continue
        graphs = bp.get_editor_property("ubergraph_pages") if False else None
        classes = {}
        node_names = []
        # Walk all loaded objects inside the blueprint package for anim graph nodes
        pkg_name = bp.get_outermost().get_name()
        for obj in unreal.get_objects_of_class(unreal.AnimGraphNode_Base, outer=bp):
            cn = obj.get_class().get_name()
            classes[cn] = classes.get(cn, 0) + 1
            node_names.append(cn + ":" + obj.get_name())
        entry = {"node_classes": classes, "status": str(bp.get_editor_property("status")),
                 "parent": str(unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp))}
        # linked anim layers / interfaces
        report["anim_graph_nodes"][path] = entry
    except Exception as exc:
        report["errors"].append(path + ": " + str(exc))

# 2) Montage notify inspection through several property paths
for path in ["/Game/DTIntegration/Animation/Combat/DT_Sword_And_Shield_Slash_mixamo_com_Montage_UEFN",
             "/Game/Art/3D/Player/Animations/Anims/Retargeted/Standing_1H_Cast_Spell_01_mixamo_com_Montage",
             "/Game/Art/3D/Player/Animations/Anims/Retargeted/Sword_And_Shield_Slash_mixamo_com_Montage"]:
    try:
        m = unreal.load_asset(path)
        if not m:
            continue
        info = {}
        for prop in ("notifies", "anim_notify_tracks", "slot_animation_tracks", "sync_markers"):
            try:
                v = m.get_editor_property(prop)
                info[prop] = str(v)[:600]
            except Exception as e:
                info[prop] = "ERR " + str(e)
        report["montage_notify_detail"][path] = info
    except Exception as exc:
        report["errors"].append(path + ": " + str(exc))

# 3) AI anim assets and spell VFX
try:
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    for ad in reg.get_assets_by_path("/Game/Art/3D/Ai", True):
        report["ai_anims"].append(str(ad.package_name) + " [" + str(ad.asset_class_path.asset_name) + "]")
    for ad in reg.get_assets_by_path("/Game/Art/Niagara", True):
        report["niagara"].append(str(ad.package_name))
except Exception as exc:
    report["errors"].append("registry: " + str(exc))

# 4) DealDamage notify graph
try:
    unreal.DTStructRepairLibrary.dump_blueprint_graph("/Game/Blueprints/Notify/BP_DealDamageNotify")
    unreal.DTStructRepairLibrary.dump_blueprint_graph("/Game/Blueprints/Actions/Melee/BP_RPGAction_MeleeAttack")
    unreal.DTStructRepairLibrary.dump_blueprint_graph("/Game/Blueprints/Actions/Melee/BP_Action_Swing")
    report["deal_damage_notify"] = "dumped to log"
except Exception as exc:
    report["errors"].append("dump: " + str(exc))

out = Path(unreal.Paths.project_saved_dir()) / "Automation" / "AnimBPNotifyAudit.json"
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(report, indent=2, default=str), encoding="utf-8")
unreal.log("AUDIT2_DONE")
unreal.SystemLibrary.quit_editor()
