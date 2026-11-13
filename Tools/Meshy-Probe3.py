"""Probe #3: bone trees, ref pose, socket API, retargeter global settings."""
import json
from pathlib import Path
import unreal

report = {"bones": {}, "refpose": {}, "socket": {}, "global_settings": {},
          "ikrig": {}, "errors": []}


def safe(fn, *a, **k):
    try:
        return fn(*a, **k)
    except Exception as exc:
        return "ERR:" + str(exc)


def enum_bones(mesh):
    """DFS bone enumeration via get_bone_children / get_bone_parent."""
    out = {}
    # try to discover roots
    roots = []
    for cand in ("root", "Root", "pelvis", "Pelvis", "mixamorig:Hips", "Armature",
                 "Hips", "Bip01", "headfront", "Mesh_0"):
        try:
            p = mesh.get_bone_parent(unreal.Name(cand))
            if p is not None:
                out.setdefault("probe_roots", {})[cand] = str(p)
                if str(p) in ("None", ""):
                    roots.append(cand)
        except Exception as e:
            out.setdefault("probe_roots", {})[cand] = "ERR:" + str(e)
    bones = []
    def dfs(bone, depth):
        bones.append(bone)
        try:
            kids = mesh.get_bone_children(unreal.Name(bone), False)
            for k in kids:
                dfs(str(k), depth + 1)
        except Exception:
            pass
    for r in roots:
        dfs(r, 0)
    if not bones:
        # brute force: walk up from a known leaf
        try:
            chain = safe(unreal.AnimationLibrary.find_bone_path_to_root, mesh, unreal.Name("mixamorig:HeadTop_End"))
            out["find_bone_path_to_root"] = str(chain)[:2000]
        except Exception as e:
            out["fp_err"] = str(e)
    out["roots"] = roots
    out["bones"] = bones
    out["count"] = len(bones)
    return out


for label, path in [
    ("lichking", "/Game/DuTraveller/Characters/Imported/Meshy/AzureLichKing/SK_Meshy_AzureLichKing"),
    ("serpent", "/Game/Characters/Serpent/Meshy_AI_Serpent_Sovereign_20261103_animated_ue5_mannequin_Running"),
    ("ue4", "/Game/Characters/Mannequin_UE4/Meshes/SK_Mannequin"),
    ("uefn", "/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin"),
]:
    mesh = unreal.load_asset(path)
    if not mesh:
        report["bones"][label] = {"missing": True}
        continue
    report["bones"][label] = enum_bones(mesh)
    report["bones"][label]["skeleton"] = safe(lambda m=mesh: m.get_editor_property("skeleton").get_path_name())
    report["bones"][label]["bounds"] = str(safe(mesh.get_imported_bounds))
    report["bones"][label]["num_sockets"] = safe(mesh.get_editor_property, "num_sockets")

    skel = safe(mesh.get_editor_property, "skeleton")
    if skel and not isinstance(skel, str):
        rp = safe(skel.get_reference_pose)
        report["refpose"][label] = str(rp)[:600]

# socket API: try variants on lich king (test bone mixamorig:RightHand)
try:
    mesh = unreal.load_asset("/Game/DuTraveller/Characters/Imported/Meshy/AzureLichKing/SK_Meshy_AzureLichKing")
    for attempt, args in (
        ("add_socket(name,bone)", dict(socket_name=unreal.Name("hand_r"), bone_name=unreal.Name("mixamorig:RightHand"))),
        ("add_socket(bone,name)", dict(bone_name=unreal.Name("mixamorig:RightHand"), socket_name=unreal.Name("hand_r"))),
    ):
        try:
            r = mesh.add_socket(**args)
            report["socket"][attempt[0]] = str(r)
            break
        except Exception as e:
            report["socket"][attempt[0]] = "ERR:" + str(e)
    report["socket"]["find_socket"] = str(safe(mesh.find_socket, unreal.Name("hand_r")))
    report["socket"]["num_sockets_after"] = str(safe(mesh.get_editor_property, "num_sockets"))
    # signature help
    try:
        report["socket"]["add_socket_doc"] = str(mesh.add_socket.__doc__)[:800]
    except Exception as e:
        report["socket"]["doc_err"] = str(e)
except Exception as exc:
    report["socket"]["outer"] = str(exc)

# retargeter global settings shape
try:
    rtg = unreal.load_asset("/Game/DTIntegration/Animation/RTG_DT_LegacyUE4_UEFN")
    ctrl = unreal.IKRetargeterController.get_controller(rtg)
    gs = safe(ctrl.get_global_settings)
    report["global_settings"]["raw"] = str(gs)[:1500]
    try:
        report["global_settings"]["dict"] = gs.to_dict() if hasattr(gs, "to_dict") else str(dir(gs))
    except Exception as e:
        report["global_settings"]["dict"] = "ERR:" + str(e)
    report["global_settings"]["num_ops"] = str(safe(ctrl.get_num_retarget_ops))
    names = []
    for i in range(10):
        try:
            names.append(str(ctrl.get_op_name(i)))
        except Exception:
            break
    report["global_settings"]["op_names"] = names
    report["global_settings"]["poses"] = str(safe(ctrl.get_retarget_poses))[:800]
except Exception as exc:
    report["global_settings"]["err"] = str(exc)

# IKRigDefinition internals on existing rig
try:
    rig = unreal.load_asset("/Game/DTIntegration/Animation/IK_DT_UEFN")
    report["ikrig"]["props"] = {}
    for p in ("solvers", "retarget_definition", "preview_skeletal_mesh", "controller"):
        report["ikrig"]["props"][p] = str(safe(rig.get_editor_property, p))[:400]
    rctrl = unreal.IKRigController.get_controller(rig)
    report["ikrig"]["chains"] = str(safe(rctrl.get_retarget_chains))[:2000]
    report["ikrig"]["root_bone"] = str(safe(rctrl.get_root_bone))
    report["ikrig"]["retarget_root"] = str(safe(rctrl.get_retarget_root))
    report["ikrig"]["ref_pose_pelvis"] = str(safe(rctrl.get_ref_pose_transform_of_bone, unreal.Name("pelvis")))[:400]
except Exception as exc:
    report["ikrig"]["err"] = str(exc)

out = Path(unreal.Paths.project_saved_dir()) / "Automation" / "MeshyProbe3.json"
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(report, indent=2, default=str), encoding="utf-8")
unreal.log("MESHY_PROBE3_DONE")
unreal.SystemLibrary.quit_editor()
