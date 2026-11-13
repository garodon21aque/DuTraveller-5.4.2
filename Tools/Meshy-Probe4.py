"""Probe #4: signatures + bone enumeration fix + socket creation on the lich mesh."""
import json
from pathlib import Path
import unreal

report = {"docs": {}, "bones": {}, "socket": {}, "rig": {}, "errors": []}


def safe(fn, *a, **k):
    try:
        return fn(*a, **k)
    except Exception as exc:
        return "ERR:" + str(exc)


mesh = unreal.load_asset("/Game/DuTraveller/Characters/Imported/Meshy/AzureLichKing/SK_Meshy_AzureLichKing")

for label, fn in (
    ("get_bone_children", mesh.get_bone_children),
    ("get_bone_parent", mesh.get_bone_parent),
    ("find_socket", mesh.find_socket),
    ("add_socket", mesh.add_socket),
    ("find_bone_path_to_root", unreal.AnimationLibrary.find_bone_path_to_root),
    ("does_bone_name_exist", unreal.AnimationLibrary.does_bone_name_exist),
    ("SkeletalMeshSocket", unreal.SkeletalMeshSocket),
    ("add_retarget_chain", unreal.IKRigController.add_retarget_chain),
    ("set_retarget_root", unreal.IKRigController.set_retarget_root),
    ("set_root_motion_bone", unreal.IKRigController.set_root_motion_bone),
    ("get_retarget_chains", unreal.IKRigController.get_retarget_chains),
    ("spawn_tab_gen_class", unreal.EditorUtilitySubsystem.spawn_and_register_tab_generated_class),
    ("try_run_class", unreal.EditorUtilitySubsystem.try_run_class),
    ("duplicate_and_retarget", unreal.IKRetargetBatchOperation.duplicate_and_retarget),
):
    try:
        report["docs"][label] = str(fn.__doc__)[:700]
    except Exception as e:
        report["docs"][label] = "ERR:" + str(e)

# proper bone enumeration — try single-arg get_bone_children
def enum(mesh):
    seeds = ["SmartRigArmature", "root", "pelvis", "Hips", "mixamorig:Hips"]
    out = {"order": [], "parents": {}}
    def kids(b):
        for args in ((b,), (unreal.Name(b),)):
            try:
                return [str(x) for x in mesh.get_bone_children(*args)]
            except Exception:
                pass
        for args in ((unreal.Name(b), True), (unreal.Name(b), False)):
            try:
                return [str(x) for x in mesh.get_bone_children(*args)]
            except Exception:
                pass
        return []
    seen = set()
    def dfs(b):
        if b in seen:
            return
        seen.add(b)
        out["order"].append(b)
        for k in kids(b):
            out["parents"][k] = b
            dfs(k)
    for s in seeds:
        c = kids(s)
        if c:
            dfs(s)
    # fallback: chain walk from Hips upward
    if not out["order"]:
        p = mesh.get_bone_parent(unreal.Name("Hips"))
        while p is not None and str(p) not in ("None", ""):
            cur = str(p)
            p = mesh.get_bone_parent(unreal.Name(cur))
            out["order"].insert(0, cur)
        if out["order"]:
            dfs(out["order"][0])
    return out

report["bones"]["lich"] = enum(mesh)

serpent = unreal.load_asset("/Game/Characters/Serpent/Meshy_AI_Serpent_Sovereign_20261103_animated_ue5_mannequin_Running")
report["bones"]["serpent"] = enum(serpent)

# does_bone_name_exist check
report["bones"]["lich_hips_exists"] = str(safe(unreal.AnimationLibrary.does_bone_name_exist, mesh, "Hips"))
report["bones"]["lich_hips_F"] = str(safe(unreal.AnimationLibrary.does_bone_name_exist, mesh, unreal.Name("Hips")))

# socket creation variants
def _props_sock():
    s = unreal.SkeletalMeshSocket()
    s.set_editor_property("socket_name", unreal.Name("hand_r"))
    s.set_editor_property("bone_name", unreal.Name("RightHand"))
    return s

sock = None
for desc, make in (
    ("ctor_positional", lambda: unreal.SkeletalMeshSocket(unreal.Name("hand_r"), unreal.Name("RightHand"))),
    ("ctor_kwargs", lambda: unreal.SkeletalMeshSocket(socket_name=unreal.Name("hand_r"), bone_name=unreal.Name("RightHand"))),
    ("props", lambda: _props_sock()),
):
    try:
        sock = make()
        report["socket"][desc] = "OK " + str(sock)
        break
    except Exception as e:
        report["socket"][desc] = "ERR:" + str(e)

if sock:
    try:
        mesh.add_socket(sock, False)
        report["socket"]["added"] = True
        unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
        report["socket"]["saved"] = True
    except Exception as e:
        report["socket"]["added"] = "ERR:" + str(e)
    report["socket"]["find_after"] = str(safe(mesh.find_socket, unreal.Name("hand_r")))

# existing rig state on lich rig
try:
    rig = unreal.load_asset("/Game/DuTraveller/Animations/IKRigs/IK_Meshy_AzureLichKing")
    ctrl = unreal.IKRigController.get_controller(rig)
    report["rig"]["retarget_root"] = str(safe(ctrl.get_retarget_root))
    report["rig"]["root_motion_bone"] = str(safe(ctrl.get_root_motion_bone))
    report["rig"]["chains_detail"] = [
        {"name": str(c.get_editor_property("chain_name")),
         "start": str(c.get_editor_property("start_bone")),
         "end": str(c.get_editor_property("end_bone"))}
        for c in ctrl.get_retarget_chains()][:25]
except Exception as e:
    report["rig"]["err"] = str(e)

out = Path(unreal.Paths.project_saved_dir()) / "Automation" / "MeshyProbe4.json"
out.write_text(json.dumps(report, indent=2, default=str), encoding="utf-8")
unreal.log("MESHY_PROBE4_DONE")
unreal.SystemLibrary.quit_editor()
