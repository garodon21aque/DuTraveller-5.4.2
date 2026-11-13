"""One-shot inspection for the Meshy retargeting pipeline.

Never saves or modifies any asset. Writes Saved/Automation/MeshyProbe.json.
"""
import inspect
import json
from pathlib import Path
import unreal

report = {"api": {}, "meshes": {}, "characters": {}, "animbp": {}, "manny": {},
          "source_library": {}, "plugins": {}, "errors": []}


def safe(fn, *a, **k):
    try:
        return fn(*a, **k)
    except Exception as exc:
        return "ERR:" + str(exc)


def path_of(obj):
    try:
        return obj.get_path_name() if obj else None
    except Exception:
        return str(obj)


def bone_names(skel, limit=400):
    try:
        ref = skel.get_editor_property("reference_skeleton")
        names = []
        try:
            infos = ref.get_editor_property("final_ref_bone_info") if hasattr(ref, "get_editor_property") else None
            if infos:
                names = [str(i.get_editor_property("name")) for i in infos]
        except Exception:
            pass
        if not names:
            tree = skel.get_editor_property("bone_tree")
            names = [str(t) for t in tree]
        return names[:limit]
    except Exception as exc:
        return "ERR:" + str(exc)


def describe_skel_mesh(mesh_path):
    mesh = unreal.load_asset(mesh_path)
    if not mesh:
        return {"missing": True}
    out = {"class": mesh.get_class().get_name()}
    skel = safe(mesh.get_editor_property, "skeleton")
    out["skeleton"] = path_of(skel)
    if skel:
        out["bones"] = bone_names(skel)
        out["bone_count"] = len(out["bones"]) if isinstance(out["bones"], list) else 0
    sockets = safe(lambda m: [s.get_name() for s in m.get_editor_property("sockets")], mesh)
    out["sockets"] = sockets
    bounds = safe(lambda m: str(m.get_editor_property("imported_bounds")), mesh)
    out["imported_bounds"] = bounds
    out["lod_count"] = safe(lambda m: len(m.get_editor_property("lod_info")), mesh)
    return out


# ---------------------------------------------------------------- Python API
api = report["api"]
for cls_name in ("EditorUtilityWidgetBlueprintFactory", "IKRigDefinitionFactory",
                 "IKRetargetFactory", "IKRetargetBatchOperation", "IKRetargeterController",
                 "IKRigController", "RetargetOpStackController", "InterchangeManager",
                 "EditorUtilityWidgetBlueprint", "WidgetBlueprintFactory",
                 "BlueprintEditorLibrary", "AnimationBlueprintLibrary",
                 "AssetImportTask", "SkeletalMeshImportData"):
    cls = getattr(unreal, cls_name, None)
    api[cls_name] = {"exists": cls is not None}
    if cls is not None:
        try:
            api[cls_name]["methods"] = [m for m in dir(cls) if not m.startswith("_")][:80]
        except Exception:
            pass

try:
    sig = inspect.signature(unreal.IKRetargetBatchOperation.run_batch_retarget)
    api["run_batch_retarget_sig"] = str(sig)
except Exception as exc:
    api["run_batch_retarget_sig"] = "ERR:" + str(exc)

try:
    api["inputs_props"] = [p for p in dir(unreal.IKRetargetBatchOperationInputs) if not p.startswith("_")][:60]
except Exception as exc:
    api["inputs_props"] = "ERR:" + str(exc)

# ---------------------------------------------------------------- meshes
try:
    for p in ["/Game/MeshyImports/Import_20261104_010152/Azure_Lich_King_Import0003",
          "/Game/MeshyImports/Import_20261104_010152/Azure_Lich_King_Import0003_Skeleton",
          "/Game/Characters/Serpent/Meshy_AI_Serpent_Sovereign_20261103_animated_ue5_mannequin_Running",
          "/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin",
          "/Game/Characters/Mannequin_UE4/Meshes/SK_Mannequin",
          "/Game/Art/3D/Ai/Enemy/Elf_Mage_UE4",
          "/Game/Art/3D/Ai/Enemy/Elf_Melee_UE4",
          "/Game/Art/3D/Ai/Enemy/Monsters/Goliath/Goliath",
          "/Game/Art/3D/Ai/Enemy/Skeletons/Skeleton_animated"]:
        report["meshes"][p] = describe_skel_mesh(p)
except Exception as exc:
    report["errors"].append("meshes: " + str(exc))

# ---------------------------------------------------------------- characters
def describe_character(bp_path):
    bp = unreal.load_asset(bp_path)
    if not bp:
        return {"missing": True}
    gen = bp.generated_class()
    cdo = unreal.get_default_object(gen)
    out = {"parent": str(safe(unreal.BlueprintEditorLibrary.get_blueprint_parent_class, bp))}
    mesh = safe(cdo.get_editor_property, "mesh")
    if isinstance(mesh, str):
        out["mesh_err"] = mesh
        mesh = None
    if mesh:
        sm = safe(mesh.get_editor_property, "skeletal_mesh_asset") or safe(mesh.get_editor_property, "skeletal_mesh")
        out["skeletal_mesh"] = path_of(sm)
        if sm:
            sk = safe(sm.get_editor_property, "skeleton")
            out["skeleton"] = path_of(sk)
            out["sockets_on_mesh_component"] = {n: safe(mesh.does_socket_exist, n) for n in
                                                ("hand_r", "hand_l", "weapon_r", "weapon_l", "weapon",
                                                 "weapon_socket", "spell_socket", "spine_03", "head")}
        ac = safe(mesh.get_editor_property, "anim_class") or safe(mesh.get_editor_property, "anim_instance_class")
        out["anim_class"] = path_of(ac)
    router = safe(cdo.get_editor_property, "animation_router")
    if router:
        ca = {}
        try:
            for k, v in router.get_editor_property("combat_animations").items():
                m = v.get_editor_property("montage")
                ca[str(k)] = {"montage": path_of(m), "section": str(v.get_editor_property("start_section")),
                              "rate": safe(v.get_editor_property, "play_rate")}
        except Exception as exc:
            ca["ERR"] = str(exc)
        out["combat_animations"] = ca
        hr = safe(lambda r: r.get_editor_property("hit_reaction").get_editor_property("montage"), router)
        out["hit_reaction"] = path_of(hr)
    for prop in ("ai_controller_class", "auto_possess_ai", "faction_id", "enemy_class",
                 "starting_equipment", "loot_pickup_class", "loot_item_class", "experience_reward",
                 "npc_role"):
        v = safe(cdo.get_editor_property, prop)
        out[prop] = str(v)
    caps = safe(cdo.get_editor_property, "capsule_component")
    if caps:
        out["capsule_half_height"] = safe(caps.get_editor_property, "capsule_half_height")
        out["capsule_radius"] = safe(caps.get_editor_property, "capsule_radius")
    cm = safe(cdo.get_editor_property, "character_movement")
    if cm:
        out["walk_speed"] = safe(cm.get_editor_property, "max_walk_speed")
    return out


for p in ["/Game/Blueprints/AI/BP_RPGAICharacter",
          "/Game/Blueprints/AI/BP_AIRangedCharacter",
          "/Game/Blueprints/AI/BP_CombatTest_Enemy",
          "/Game/Blueprints/AI/BP_Enemy_Serpent",
          "/Game/Blueprints/AI/BP_RPGAIController"]:
    try:
        report["characters"][p] = describe_character(p)
    except Exception as exc:
        report["characters"][p] = {"ERR": str(exc)}

# ---------------------------------------------------------------- anim bp
for p in ["/Game/Art/3D/Player/Animations/Anims/AnimBP_RPGAICharacter",
          "/Game/Art/3D/Player/Animations/Anims/AnimBP_RPGCharacter",
          "/Game/Blueprints/Animations/ABP_SandboxCharacter"]:
    bp = unreal.load_asset(p)
    if not bp:
        report["animbp"][p] = {"missing": True}
        continue
    entry = {"parent": str(safe(unreal.BlueprintEditorLibrary.get_blueprint_parent_class, bp)),
             "status": str(safe(bp.get_editor_property, "status"))}
    entry["target_skeleton"] = path_of(safe(bp.get_editor_property, "target_skeleton"))
    report["animbp"][p] = entry

# ---------------------------------------------------------------- manny / fn probe
reg = unreal.AssetRegistryHelpers.get_asset_registry()
opts = unreal.AssetRegistryDependencyOptions()
ar = reg
try:
    found = []
    for ad in ar.get_assets_by_class(unreal.TopLevelAssetPath("/Script/Engine", "SkeletalMesh"), True):
        pn = str(ad.package_name)
        if "manny" in pn.lower() or "quinn" in pn.lower():
            found.append(pn)
    report["manny"]["skeletal_meshes_all"] = found
except Exception as exc:
    report["manny"]["scan_err"] = str(exc)

for test_path in ["/MoverExamples/Characters/Mannequins/Meshes/SKM_Manny",
                  "/MoverExamples/Characters/Mannequins/Meshes/SKM_Manny_Simple",
                  "/MoverExamples/Characters/Mannequins/Rigs/IK_Mannequin",
                  "/MoverExamples/Characters/Mannequins/Rigs/RTG_Mannequin",
                  "/MoverExamples/Characters/Mannequins/Animations/Manny/MM_Idle"]:
    report["manny"][test_path] = {
        "exists": safe(unreal.EditorAssetLibrary.does_asset_exist, test_path),
        "loads": path_of(safe(unreal.load_asset, test_path))}

# engine plugin mount check
try:
    import os
    ue = os.environ.get("UE_ROOT", r"C:\Program Files\Epic Games\UE_5.8")
    mp = Path(ue) / "Engine/Plugins/Experimental/MoverExamples/Content/Characters/Mannequins"
    report["manny"]["plugin_content_on_disk"] = mp.exists()
except Exception as exc:
    report["manny"]["disk_err"] = str(exc)

for plug in ("MoverExamples", "PythonScriptPlugin", "EditorScriptingUtilities", "Blutility", "Interchange"):
    report["plugins"][plug] = str(safe(unreal.PluginsEditorSubsystem.is_plugin_enabled, plug)) \
        if hasattr(unreal, "PluginsEditorSubsystem") else "no-api"

# ---------------------------------------------------------------- source library (UE4 skeleton)
try:
    lib = {"AnimSequence": [], "AnimMontage": [], "BlendSpace": [], "AimOffsetBlendSpace": [], "AnimBlueprint": []}
    for ad in ar.get_assets_by_path("/Game", True):
        cls = str(ad.asset_class_path.asset_name)
        if cls in lib:
            pn = str(ad.package_name)
            if any(k in pn for k in ("Retargeted", "Ai/Enemy", "Animations", "Mixamo")):
                lib[cls].append(pn)
    report["source_library"]["by_class"] = {k: len(v) for k, v in lib.items()}
    report["source_library"]["montages"] = lib["AnimMontage"]
    report["source_library"]["blendspaces"] = lib["BlendSpace"] + lib["AimOffsetBlendSpace"]
except Exception as exc:
    report["errors"].append("library scan: " + str(exc))

out = Path(unreal.Paths.project_saved_dir()) / "Automation" / "MeshyProbe.json"
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(report, indent=2, default=str), encoding="utf-8")
unreal.log("MESHY_PROBE_DONE " + str(out))
unreal.SystemLibrary.quit_editor()
