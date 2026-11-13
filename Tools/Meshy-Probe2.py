"""Probe #2: API surface details + skeletal re-import test of the rigged Meshy FBX.
Writes Saved/Automation/MeshyProbe2.json. Never modifies existing assets.
"""
import json
from pathlib import Path
import unreal

report = {"api": {}, "import": {}, "errors": []}


def safe(fn, *a, **k):
    try:
        return fn(*a, **k)
    except Exception as exc:
        return "ERR:" + str(exc)


def full_dir(obj, cap=300):
    try:
        return sorted(m for m in dir(obj) if not m.startswith("_"))[:cap]
    except Exception as exc:
        return "ERR:" + str(exc)


# ------------------------------------------------------------- API inventory
for name in ("Skeleton", "SkeletalMesh", "EditorSkeletalMeshLibrary",
             "SkeletalMeshEditorSubsystem", "IKRigDefinition", "EditorUtilitySubsystem",
             "EditorUtilityWidget", "AnimDataController", "AnimBlueprintFactory",
             "BlueprintEditorLibrary", "EditorAssetLibrary", "BlendSpaceFactory",
             "RetargetOpBase", "RetargetRootMotionOp", "RetargetOpStack",
             "BlendSpace", "FbxImportUI", "InterchangePipelineConfigurationBase",
             "InterchangeGenericAssetsPipeline", "InterchangeGenericMeshPipeline",
             "InterchangeGenericCommonSkeletalMeshesAndAnimationsProperties",
             "AnimationLibrary", "MeshDescription", "SocketSubsystem",
             "EditorSkeletalMeshSubsystem", "ToolMenus", "KismetEditorUtilities"):
    cls = getattr(unreal, name, None)
    report["api"][name] = {"exists": cls is not None}
    if cls is not None:
        report["api"][name]["methods"] = full_dir(cls)

# full controller method lists (were truncated in probe 1)
for name in ("IKRigController", "IKRetargeterController", "BlueprintEditorLibrary"):
    cls = getattr(unreal, name, None)
    report["api"][name + "_full"] = full_dir(cls) if cls else "missing"

# EUW factory parent_class probe
try:
    f = unreal.EditorUtilityWidgetBlueprintFactory()
    props = [p for p in dir(f) if "parent" in p or "class" in p]
    report["api"]["euw_factory_props"] = props
    report["api"]["euw_parent_class_val"] = str(safe(f.get_editor_property, "parent_class"))
except Exception as exc:
    report["api"]["euw_factory_err"] = str(exc)

# ------------------------------------------------------------- import test
root = Path(unreal.Paths.project_dir())
fbx = root / "Content" / "MeshyImports" / "Azure_Lich_King_Import0001.fbx"
dest = "/Game/DuTraveller/Characters/Imported/Meshy/AzureLichKing"
try:
    report["import"]["fbx_exists"] = fbx.exists()
    task = unreal.AssetImportTask()
    task.set_editor_properties(dict(
        filename=str(fbx),
        destination_path=dest,
        destination_name="SK_Meshy_AzureLichKing",
        automated=True,
        replace_existing=True,
        save=True,
    ))
    opts = unreal.FbxImportUI()
    for prop, val in dict(
            import_mesh=True, import_as_skeletal=True, import_animations=True,
            import_materials=True, import_textures=True,
            create_physics_asset=False, mesh_type_to_import=unreal.FBXImportType.FBXIT_SKELETAL_MESH).items():
        try:
            opts.set_editor_property(prop, val)
        except Exception as e:
            report["import"].setdefault("opt_errors", {})[prop] = str(e)
    task.set_editor_property("options", opts)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    report["import"]["imported_object_paths"] = [str(p) for p in task.get_editor_property("imported_object_paths")]
    report["import"]["result"] = str(task.get_editor_property("result"))
    mesh = None
    for p in task.get_editor_property("imported_object_paths"):
        obj = unreal.load_asset(str(p).split(".")[0])
        if isinstance(obj, unreal.SkeletalMesh):
            mesh = obj
            break
    if mesh:
        report["import"]["mesh"] = mesh.get_path_name()
        skel = mesh.get_editor_property("skeleton")
        report["import"]["skeleton"] = skel.get_path_name() if skel else None
        # bone enumeration attempts
        attempts = {}
        attempts["skel_dir_boneish"] = [m for m in dir(skel) if "bone" in m.lower() or "tree" in m.lower()]
        try:
            skel_info = skel.get_editor_property("bone_tree")
            attempts["bone_tree"] = str(skel_info)[:400]
        except Exception as e:
            attempts["bone_tree"] = "ERR:" + str(e)
        try:
            ref = mesh.get_editor_property("ref_skeleton")
            attempts["mesh_ref_skeleton"] = str(ref)[:400]
        except Exception as e:
            attempts["mesh_ref_skeleton"] = "ERR:" + str(e)
        try:
            ctrl = unreal.IKRigController.get_controller(None)
            attempts["ikctrl_null"] = str(ctrl)
        except Exception as e:
            attempts["ikctrl_null"] = "ERR:" + str(e)
        report["import"]["bone_attempts"] = attempts
    else:
        report["import"]["no_skeletal_mesh"] = True
except Exception:
    import traceback
    report["import"]["exception"] = traceback.format_exc()

out = Path(unreal.Paths.project_saved_dir()) / "Automation" / "MeshyProbe2.json"
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(report, indent=2, default=str), encoding="utf-8")
unreal.log("MESHY_PROBE2_DONE")
unreal.SystemLibrary.quit_editor()
