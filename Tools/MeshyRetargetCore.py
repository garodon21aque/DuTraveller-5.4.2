"""MeshyRetargetCore — shared library for the DuTraveller Meshy enemy pipeline.

Everything here runs inside UnrealEditor (Python 3.11, `unreal` module).
Entry points used by the Meshy-*.py runner scripts and by
EUW_DuTraveller_CharacterRetargeter / the native DuTraveller retargeter tab.

Content layout (per Docs/MESHY_RETARGETING.md):
  /Game/DuTraveller/Characters/Imported/Meshy/<Name>/   imported FBX assets
  /Game/DuTraveller/Animations/IKRigs/                  IK_<Source|Meshy_*> rigs
  /Game/DuTraveller/Animations/Retargeters/             RTG_*_<Meshy_*> retargeters
  /Game/DuTraveller/Animations/Retargeted/<Name>/       retargeted animation library
  /Game/DuTraveller/Animations/EnemyProfiles/           per-character JSON profiles (Saved)
  /Game/DuTraveller/Editor/Retargeting/                 EUW asset
"""
import json
import math
import os
import traceback
from pathlib import Path

import unreal

# ------------------------------------------------------------------ constants
IMPORT_ROOT = "/Game/DuTraveller/Characters/Imported/Meshy"
RIG_ROOT = "/Game/DuTraveller/Animations/IKRigs"
RTG_ROOT = "/Game/DuTraveller/Animations/Retargeters"
OUT_ROOT = "/Game/DuTraveller/Animations/Retargeted"
EDITOR_ROOT = "/Game/DuTraveller/Editor/Retargeting"
ENEMY_BP_ROOT = "/Game/Blueprints/AI"

UE4_MANNEQUIN_MESH = "/Game/Characters/Mannequin_UE4/Meshes/SK_Mannequin"
UEFN_MESH = "/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin"
SERPENT_MESH = "/Game/Characters/Serpent/Meshy_AI_Serpent_Sovereign_20261103_animated_ue5_mannequin_Running"
IK_DT_LegacyUE4 = "/Game/DTIntegration/Animation/IK_DT_LegacyUE4"
IK_DT_UEFN = "/Game/DTIntegration/Animation/IK_DT_UEFN"

ENEMY_ANIMBP = "/Game/Art/3D/Player/Animations/Anims/AnimBP_RPGAICharacter"
ENEMY_PARENT_BP = "/Game/Blueprints/AI/BP_RPGAICharacter"
ENEMY_CONTROLLER_BP = "/Game/Blueprints/AI/BP_CombatTest_Controller"

# Enemy combat library on the UE4 mannequin skeleton that gets retargeted.
# Kept deliberately small — the AnimBP + montages pull their sequence
# dependencies automatically via include_referenced_assets.
ROUTE_A_LIBRARY = [
    "/Game/Art/3D/Player/Animations/Anims/Retargeted/Sword_And_Shield_Slash_mixamo_com_Montage",
    "/Game/Art/3D/Player/Animations/Anims/Retargeted/Standing_1H_Cast_Spell_01_mixamo_com_Montage",
    "/Game/Art/3D/Ai/Enemy/Anims/AMAI_ReactHitFont",
    "/Game/Art/3D/Ai/Enemy/Anims/AMAI_ReactHit",
    "/Game/Art/3D/Animations/Mage/AM_Mage_1HCast",
    "/Game/Art/3D/Animations/Warrior/AM_Warrior_Axe_DownSlash",
    "/Game/Art/3D/Player/Animations/Anims/Retargeted/Falling_Back_Death_mixamo_com_Montage",
    "/Game/Art/3D/Player/Animations/Anims/Retargeted/Falling_Forward_Death_mixamo_com_Montage",
    "/Game/Art/3D/Player/Animations/Anims/LocomotionBS",
]
# Extra candidates for the state machine; filtered by existence at runtime.
ROUTE_A_ANIMBP_SEQUENCES = [
    "/Game/Art/3D/Player/Animations/Anims/Retargeted/Getting_Up_mixamo_com",
    "/Game/Art/3D/Player/Animations/Anims/Retargeted/Falling_mixamo_com",
    "/Game/Art/3D/Player/Animations/Anims/Retargeted/Sleeping_Idle",
    "/Game/Art/3D/Player/Animations/Anims/Retargeted/Unarmed_Jump_Running",
    "/Game/Art/3D/Player/Animations/Anims/Retargeted/OversideJump_mixamo_com",
]
# FN (UEFN) animation subset used for Route C retargets.
ROUTE_C_LIBRARY = [
    "/Game/Characters/UEFN_Mannequin/Animations/Idle/M_Neutral_Stand_Idle_Loop",
    "/Game/Characters/UEFN_Mannequin/Animations/Jump/M_Neutral_Jump_Loop_Fall",
    "/Game/Characters/UEFN_Mannequin/Animations/Jump/M_Neutral_Jump_B_Land_Stand_Light",
]
# UE5 Manny assets live in the (disabled-by-default) MoverExamples plugin.
ROUTE_B_MANNY_CANDIDATES = [
    "/MoverExamples/Characters/Mannequins/Meshes/SKM_Manny",
    "/MoverExamples/Characters/Mannequins/Meshes/SKM_Manny_Simple",
]
ROUTE_B_MANNY_RIG = "/MoverExamples/Characters/Mannequins/Rigs/IK_Mannequin"
ROUTE_B_MANNY_ANIM_DIR = "/MoverExamples/Characters/Mannequins/Animations/Manny"

SOCKET_MAP = {
    # socket_name -> list of bone candidates (first match wins)
    "hand_r": ["mixamorig:RightHand", "RightHand", "hand_r", "Right_Hand", "RightHandBone"],
    "hand_l": ["mixamorig:LeftHand", "LeftHand", "hand_l", "Left_Hand", "LeftHandBone"],
    "weapon_r": ["mixamorig:RightHand", "RightHand", "hand_r", "Right_Hand"],
    "head": ["mixamorig:Head", "Head", "head"],
    "tail": ["mixamorig:Tail", "Tail", "tail", "tail_01", "Bone_Tail"],
}

# Canonical humanoid retarget chains.  {chain: (start, end, goal or None)}
CHAINS_MIXAMO = {
    "Root": ("mixamorig:Hips", "mixamorig:Hips", None),
    "Spine": ("mixamorig:Hips", "mixamorig:Spine2", None),
    "Neck": ("mixamorig:Neck", "mixamorig:Neck", None),
    "Head": ("mixamorig:Head", "mixamorig:HeadTop_End", None),
    "LeftShoulder": ("mixamorig:LeftShoulder", "mixamorig:LeftArm", None),
    "LeftArm": ("mixamorig:LeftArm", "mixamorig:LeftHand", "LeftHandIK"),
    "LeftHand": ("mixamorig:LeftHand", "mixamorig:LeftHand", None),
    "LeftIndex": ("mixamorig:LeftHandIndex1", "mixamorig:LeftHandIndex3", None),
    "LeftMiddle": ("mixamorig:LeftHandMiddle1", "mixamorig:LeftHandMiddle3", None),
    "LeftRing": ("mixamorig:LeftHandRing1", "mixamorig:LeftHandRing3", None),
    "LeftPinky": ("mixamorig:LeftHandPinky1", "mixamorig:LeftHandPinky3", None),
    "LeftThumb": ("mixamorig:LeftHandThumb1", "mixamorig:LeftHandThumb3", None),
    "RightShoulder": ("mixamorig:RightShoulder", "mixamorig:RightArm", None),
    "RightArm": ("mixamorig:RightArm", "mixamorig:RightHand", "RightHandIK"),
    "RightHand": ("mixamorig:RightHand", "mixamorig:RightHand", None),
    "RightIndex": ("mixamorig:RightHandIndex1", "mixamorig:RightHandIndex3", None),
    "RightMiddle": ("mixamorig:RightHandMiddle1", "mixamorig:RightHandMiddle3", None),
    "RightRing": ("mixamorig:RightHandRing1", "mixamorig:RightHandRing3", None),
    "RightPinky": ("mixamorig:RightHandPinky1", "mixamorig:RightHandPinky3", None),
    "RightThumb": ("mixamorig:RightHandThumb1", "mixamorig:RightHandThumb3", None),
    "LeftLeg": ("mixamorig:LeftUpLeg", "mixamorig:LeftFoot", "LeftFootIK"),
    "LeftFoot": ("mixamorig:LeftFoot", "mixamorig:LeftToeBase", None),
    "RightLeg": ("mixamorig:RightUpLeg", "mixamorig:RightFoot", "RightFootIK"),
    "RightFoot": ("mixamorig:RightFoot", "mixamorig:RightToeBase", None),
}
CHAINS_UE = {
    "Root": ("pelvis", "pelvis", None),
    "Spine": ("pelvis", "spine_03", None),
    "Neck": ("neck_01", "neck_01", None),
    "Head": ("head", "head", None),
    "LeftClavicle": ("clavicle_l", "upperarm_l", None),
    "LeftArm": ("upperarm_l", "hand_l", "LeftHandIK"),
    "LeftHand": ("hand_l", "hand_l", None),
    "LeftIndex": ("index_01_l", "index_03_l", None),
    "LeftMiddle": ("middle_01_l", "middle_03_l", None),
    "LeftRing": ("ring_01_l", "ring_03_l", None),
    "LeftPinky": ("pinky_01_l", "pinky_03_l", None),
    "LeftThumb": ("thumb_01_l", "thumb_03_l", None),
    "RightClavicle": ("clavicle_r", "upperarm_r", None),
    "RightArm": ("upperarm_r", "hand_r", "RightHandIK"),
    "RightHand": ("hand_r", "hand_r", None),
    "RightIndex": ("index_01_r", "index_03_r", None),
    "RightMiddle": ("middle_01_r", "middle_03_r", None),
    "RightRing": ("ring_01_r", "ring_03_r", None),
    "RightPinky": ("pinky_01_r", "pinky_03_r", None),
    "RightThumb": ("thumb_01_r", "thumb_03_r", None),
    "LeftLeg": ("thigh_l", "foot_l", "LeftFootIK"),
    "LeftFoot": ("foot_l", "ball_l", None),
    "RightLeg": ("thigh_r", "foot_r", "RightFootIK"),
    "RightFoot": ("foot_r", "ball_r", None),
}
REQUIRED_CANONICAL = ("Spine", "Head", "LeftArm", "RightArm", "LeftLeg", "RightLeg")


# ------------------------------------------------------------------ utilities
def safe(fn, *a, **k):
    try:
        return fn(*a, **k)
    except Exception as exc:
        return "ERR:" + str(exc)


def norm_path(path):
    """Undo MSYS-style mangling of /Game and /Engine virtual paths."""
    if not isinstance(path, str):
        return path
    for marker in ("/Game/", "/Engine/", "/Script/"):
        i = path.find(marker)
        if i > 0 and (":" in path[:i] or "Git" in path[:i]):
            return path[i:]
    return path


def load(path):
    path = norm_path(path)
    return unreal.load_asset(path) if path else None


def exists(path):
    try:
        return unreal.EditorAssetLibrary.does_asset_exist(norm_path(path))
    except Exception:
        return False


def save(asset):
    return unreal.EditorAssetLibrary.save_loaded_asset(asset, False)


def tools():
    return unreal.AssetToolsHelpers.get_asset_tools()


def create_asset(name, path, cls, factory):
    full = path + "/" + name
    if exists(full):
        return unreal.load_asset(full), False
    asset = tools().create_asset(name, path, cls, factory)
    return asset, True


def name_of(asset_or_path):
    if asset_or_path is None:
        return None
    if isinstance(asset_or_path, str):
        return asset_or_path
    try:
        return asset_or_path.get_path_name()
    except Exception:
        return str(asset_or_path)


def short(path):
    return str(path).rsplit("/", 1)[-1].split(".")[0]


# ------------------------------------------------------------------ bones
def bone_exists(mesh, bone):
    try:
        parent = mesh.get_bone_parent(unreal.Name(bone))
        if str(parent) not in ("None", ""):
            return True
        return len(bone_children(mesh, bone)) > 0
    except Exception:
        return False


def bone_children(mesh, bone):
    try:
        return [str(k) for k in mesh.get_bone_children(unreal.Name(bone))]
    except Exception:
        return []


def enumerate_bones(mesh):
    """Returns (ordered_bone_names, parent_map)."""
    seeds = ["SmartRigArmature", "root", "Root", "pelvis", "Pelvis", "Hips",
             "mixamorig:Hips", "Armature", "Bip01", "Reference", "BoneRoot"]
    visited, parents, order = set(), {}, []

    def dfs(b):
        if b in visited:
            return
        visited.add(b)
        order.append(b)
        for k in bone_children(mesh, b):
            parents[k] = b
            dfs(k)

    progressed = False
    for s in seeds:
        kids = bone_children(mesh, s)
        if kids:
            dfs(s)
            progressed = True
    if not progressed:
        # walk upward from any bone that resolves a parent
        for leaf in ("headfront", "Head", "head", "Hips", "pelvis"):
            cur, chain = leaf, []
            while cur and str(cur) not in ("None", "") and cur not in visited:
                chain.append(cur)
                try:
                    cur = str(mesh.get_bone_parent(unreal.Name(cur)))
                except Exception:
                    break
            if chain:
                dfs(chain[-1])
                progressed = True
                break
    return order, parents


def detect_convention(bones):
    lower = {b.lower(): b for b in bones}
    if any(b.startswith("mixamorig") for b in lower):
        return "mixamo"
    if "pelvis" in lower and ("clavicle_l" in lower or "upperarm_l" in lower):
        return "ue"
    if "hips" in lower and "spine" in lower:
        return "meshy"   # bare mixamo-style names (Hips/Spine/Head)
    return "generic"


def resolve_bone(bones, candidates):
    lower = {b.lower(): b for b in bones}
    for c in candidates:
        if c.lower() in lower:
            return lower[c.lower()]
    return None


def chain_table_for(convention, bones):
    """Returns {chain_name: (start, end, goal)} resolved to real bone names."""
    table = {}
    if convention == "mixamo":
        for name, (s, e, g) in CHAINS_MIXAMO.items():
            table[name] = (resolve_bone(bones, [s]), resolve_bone(bones, [e]), g)
    elif convention == "ue":
        for name, (s, e, g) in CHAINS_UE.items():
            table[name] = (resolve_bone(bones, [s, s.replace("_l", "_L")]),
                           resolve_bone(bones, [e, e.replace("_l", "_L")]), g)
    else:
        # meshy "SmartRig" bare names mirror Mixamo without the prefix.
        prefix_alt = lambda n: n.replace("mixamorig:", "")
        for name, (s, e, g) in CHAINS_MIXAMO.items():
            cands_s = [s, prefix_alt(s)]
            cands_e = [e, prefix_alt(e)]
            # meshy SmartRig uses Head/headfront instead of HeadTop_End
            if name == "Head":
                cands_e += ["HeadTop_End", "headfront", "HeadFront", "Head"]
            if name == "Root":
                cands_s += ["Hips", "pelvis"]
                cands_e += ["Hips", "pelvis"]
            if name == "Spine":
                cands_s += ["Hips"]
            if name == "LeftLeg":
                cands_s += ["LeftLeg", "LeftUpLeg", "UpperLeg_L", "Left_UpperLeg"]
                cands_e += ["LeftFoot", "Foot_L"]
            if name == "RightLeg":
                cands_s += ["RightLeg", "RightUpLeg", "UpperLeg_R", "Right_UpperLeg"]
                cands_e += ["RightFoot", "Foot_R"]
            table[name] = (resolve_bone(bones, cands_s), resolve_bone(bones, cands_e), g)
    return table


def detect_pose(rig_controller, bones, convention):
    """Estimate T vs A pose from the ref pose arm direction."""
    try:
        side = "Left" if convention != "ue" else "l"
        if convention == "ue":
            up, hand = "upperarm_l", "hand_l"
        else:
            up = resolve_bone(bones, ["mixamorig:LeftArm", "LeftArm", "UpperArm_L"])
            hand = resolve_bone(bones, ["mixamorig:LeftHand", "LeftHand", "Hand_L"])
        if not up or not hand:
            return "unknown"
        a = rig_controller.get_ref_pose_transform_of_bone(unreal.Name(up))
        b = rig_controller.get_ref_pose_transform_of_bone(unreal.Name(hand))
        d = (b.translation - a.translation)
        n = math.sqrt(d.x * d.x + d.y * d.y + d.z * d.z) or 1.0
        zfrac = abs(d.z) / n
        return "T-pose" if zfrac < 0.35 else ("A-pose" if zfrac < 0.75 else "arms-down")
    except Exception:
        return "unknown"


def analyze_skeletal_mesh(mesh_or_path):
    mesh = load(mesh_or_path) if isinstance(mesh_or_path, str) else mesh_or_path
    info = {"mesh": name_of(mesh)}
    if not mesh:
        info["error"] = "missing"
        return info
    info["class"] = mesh.get_class().get_name()
    if not isinstance(mesh, unreal.SkeletalMesh):
        info["rigged"] = False
        info["requires_rigging"] = True
        return info
    skel = safe(mesh.get_editor_property, "skeleton")
    info["skeleton"] = name_of(skel)
    bones, parents = enumerate_bones(mesh)
    info["bones"] = bones
    info["bone_count"] = len(bones)
    info["rigged"] = bool(bones) and bool(skel)
    info["requires_rigging"] = not info["rigged"]
    info["convention"] = detect_convention(bones)
    info["bounds"] = str(safe(mesh.get_imported_bounds))
    table = chain_table_for(info["convention"], bones)
    info["chains_resolved"] = {k: [s, e] for k, (s, e, g) in table.items() if s and e}
    info["chains_missing"] = [k for k, (s, e, g) in table.items() if not (s and e)]
    info["canonical_missing"] = [k for k in REQUIRED_CANONICAL if k in info["chains_missing"]]
    info["humanoid"] = len(info["canonical_missing"]) == 0
    mapped = {b for s, e in info["chains_resolved"].values() for b in (s, e)}
    info["extra_bones"] = [b for b in bones if b not in mapped][:40]
    return info


# ------------------------------------------------------------------ import
def import_meshy_fbx(fbx_path, name, dest_dir=None, import_animations=True):
    """Import a rigged Meshy FBX. Returns dict with mesh/skeleton paths."""
    dest_dir = dest_dir or (IMPORT_ROOT + "/" + name)
    task = unreal.AssetImportTask()
    task.set_editor_properties(dict(
        filename=str(fbx_path), destination_path=dest_dir,
        destination_name=name if str(name).startswith("SK_") else "SK_Meshy_" + name,
        automated=True, replace_existing=True, save=True))
    opts = unreal.FbxImportUI()
    for prop, val in dict(import_mesh=True, import_as_skeletal=True,
                          import_animations=bool(import_animations),
                          import_materials=True, import_textures=True,
                          create_physics_asset=False,
                          mesh_type_to_import=unreal.FBXImportType.FBXIT_SKELETAL_MESH).items():
        try:
            opts.set_editor_property(prop, val)
        except Exception:
            pass
    task.set_editor_property("options", opts)
    tools().import_asset_tasks([task])
    out = {"task_paths": [str(p) for p in task.get_editor_property("imported_object_paths")]}
    mesh = skel = None
    for p in out["task_paths"]:
        obj = load(str(p).split(".")[0])
        if isinstance(obj, unreal.SkeletalMesh):
            mesh = obj
        elif isinstance(obj, unreal.Skeleton):
            skel = obj
    if mesh:
        out["mesh"] = mesh.get_path_name()
        skel = skel or safe(mesh.get_editor_property, "skeleton")
        out["skeleton"] = name_of(skel)
        out["analysis"] = analyze_skeletal_mesh(mesh)
    else:
        statics = [p for p in out["task_paths"]
                   if isinstance(load(str(p).split(".")[0]), unreal.StaticMesh)]
        out["requires_rigging"] = True
        out["static_meshes"] = statics
    return out


# ------------------------------------------------------------------ IK rigs
def _strip_prefix(chain_table, bones):
    """When a rig mixes prefixed and bare names, resolve to real bones."""
    return chain_table


def ensure_ik_rig(mesh_or_path, rig_name=None, auto=True, extra_single_bone_chains=True):
    """Create/reuse an IK rig for a skeletal mesh. Returns (rig, report)."""
    mesh = load(mesh_or_path) if isinstance(mesh_or_path, str) else mesh_or_path
    rep = {"mesh": name_of(mesh)}
    if not mesh:
        rep["error"] = "missing mesh"
        return None, rep
    rig_name = rig_name or ("IK_Meshy_" + short(mesh.get_path_name()).replace("SK_Meshy_", "").replace("SK_", ""))
    rig_path = RIG_ROOT + "/" + rig_name
    if exists(rig_path):
        rig = load(rig_path)
        rep["reused"] = True
    else:
        rig = tools().create_asset(rig_name, RIG_ROOT, unreal.IKRigDefinition,
                                   unreal.IKRigDefinitionFactory())
        rep["reused"] = False
    ctrl = unreal.IKRigController.get_controller(rig)
    ctrl.set_skeletal_mesh(mesh)
    bones, _ = enumerate_bones(mesh)
    convention = detect_convention(bones)
    rep["convention"] = convention
    rep["bone_count"] = len(bones)

    if auto:
        auto_ok = safe(ctrl.apply_auto_generated_retarget_definition)
        rep["auto_definition"] = auto_ok
    chains = ctrl.get_retarget_chains()
    if len(chains) < len(REQUIRED_CANONICAL):
        # manual chain construction from the convention table
        table = chain_table_for(convention, bones)
        built = 0
        for cname, (s, e, g) in table.items():
            if not (s and e):
                continue
            try:
                if cname not in [str(c.get_editor_property("chain_name")) for c in ctrl.get_retarget_chains()]:
                    ctrl.add_retarget_chain(unreal.Name(cname), unreal.Name(s), unreal.Name(e),
                                            unreal.Name(g or ""))
                    built += 1
            except Exception as exc:
                rep.setdefault("chain_errors", {})[cname] = str(exc)
        rep["chains_built_manually"] = built
    # retarget root = pelvis/hips equivalent
    root_candidates = {"ue": ["pelvis"], "mixamo": ["mixamorig:Hips", "Hips"],
                       "meshy": ["Hips", "pelvis"], "generic": ["Hips", "pelvis", "root"]}
    root = resolve_bone(bones, root_candidates.get(convention, ["pelvis", "Hips"]))
    if root:
        try:
            ctrl.set_retarget_root(unreal.Name(root))
            rep["retarget_root"] = root
        except Exception as exc:
            rep["retarget_root_err"] = str(exc)
    try:
        ctrl.set_root_motion_bone(unreal.Name(root))
    except Exception:
        pass
    # extra bones (cape/staff/tail) get their own single-bone chains so they copy
    if extra_single_bone_chains:
        mapped = set()
        for s, e, g in chain_table_for(convention, bones).values():
            if s:
                mapped.add(s)
            if e:
                mapped.add(e)
        skip = {"smartrigarmature", "root", "mesh_0"}
        extras = [b for b in bones if b.lower() not in skip and b not in mapped
                  and not any(b.lower().endswith("_end") for _ in (0,))]
        extra_built = []
        for b in extras[:24]:
            cname = "Extra_" + b.replace(":", "_").replace(" ", "_")
            try:
                ctrl.add_retarget_chain(unreal.Name(cname), unreal.Name(b), unreal.Name(b), unreal.Name(""))
                extra_built.append(b)
            except Exception:
                pass
        rep["extra_chains"] = extra_built
    if auto:
        rep["fbik"] = str(safe(ctrl.apply_auto_fbik))
    rep["chains"] = [str(c.get_editor_property("chain_name")) for c in ctrl.get_retarget_chains()]
    rep["pose"] = detect_pose(ctrl, bones, convention)
    rep["humanoid"] = len([c for c in REQUIRED_CANONICAL
                           if c not in rep["chains"]]) == 0 or convention in ("ue", "mixamo")
    save(rig)
    rep["rig"] = rig.get_path_name()
    return rig, rep


# ------------------------------------------------------------------ retargeters
def ensure_retargeter(src_rig, dst_rig, src_mesh, dst_mesh, name=None,
                      auto_map=True, auto_align=True, root_motion="generate"):
    """Create/reuse an IK retargeter between two rigs."""
    rep = {}
    name = name or ("RTG_" + short(src_rig.get_path_name()).replace("IK_", "").replace("IK_DT_", "")
                    + "_To_" + short(dst_rig.get_path_name()).replace("IK_", ""))
    path = RTG_ROOT + "/" + name
    if exists(path):
        rtg = load(path)
        rep["reused"] = True
    else:
        rtg = tools().create_asset(name, RTG_ROOT, unreal.IKRetargeter, unreal.IKRetargetFactory())
        rep["reused"] = False
    ctrl = unreal.IKRetargeterController.get_controller(rtg)
    ctrl.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, src_rig)
    ctrl.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, dst_rig)
    ctrl.set_preview_mesh(unreal.RetargetSourceOrTarget.SOURCE, src_mesh)
    ctrl.set_preview_mesh(unreal.RetargetSourceOrTarget.TARGET, dst_mesh)
    if not safe(ctrl.get_num_retarget_ops) or str(safe(ctrl.get_num_retarget_ops)) in ("0", "None"):
        safe(ctrl.add_default_ops)
    rep["ops"] = str(safe(ctrl.get_num_retarget_ops))
    if auto_map:
        mapped = safe(ctrl.auto_map_chains, unreal.AutoMapChainType.EXACT, True)
        if "ERR" in str(mapped):
            mapped = safe(ctrl.auto_map_chains, unreal.AutoMapChainType.FUZZY, True)
        rep["auto_map"] = str(mapped)
        rep["chain_settings"] = str(safe(ctrl.get_all_chain_settings))[:2000]
    if auto_align:
        rep["auto_align"] = str(safe(ctrl.auto_align_all_bones, unreal.RetargetSourceOrTarget.TARGET))
    rep["root_motion"] = _set_root_motion(ctrl, root_motion)
    save(rtg)
    rep["retargeter"] = rtg.get_path_name()
    return rtg, rep


def _set_root_motion(ctrl, mode):
    """Best-effort root-motion configuration on the retargeter."""
    out = {}
    wanted = {"generate": "Generate", "none": "None", "maintain": "Maintain",
              "map": "MapToMeshScaled", "global": "GlobalScale"}
    label = wanted.get(str(mode).lower(), "Generate")
    # 5.8: root motion is a retarget op; try its controller properties first.
    try:
        idx = ctrl.get_index_of_op_by_name("Root Motion")
        if idx is not None and int(idx) >= 0:
            oc = ctrl.get_op_controller(int(idx))
            for prop in ("root_motion", "root_motion_mode", "motion_mode"):
                try:
                    enum_cls = getattr(unreal, "RetargetRootMotionMode", None)
                    if enum_cls is not None:
                        oc.set_editor_property(prop, getattr(enum_cls, label.upper(), None) or
                                               getattr(enum_cls, "GENERATE", None))
                    else:
                        oc.set_editor_property(prop, label)
                    out["op_root_motion"] = label
                    break
                except Exception:
                    continue
    except Exception as exc:
        out["op_err"] = str(exc)
    # also honor the global settings struct when exposed
    try:
        gs = ctrl.get_global_settings()
        for prop in ("root_motion", "enable_root_motion"):
            try:
                gs.get_editor_property(prop)
                gs.set_editor_property(prop, True if label != "None" else False
                                       if "enable" in prop else label)
                out.setdefault("global", {})[prop] = True
            except Exception:
                pass
        try:
            ctrl.set_global_settings(gs)
        except Exception:
            pass
    except Exception:
        pass
    return out or {"default": True}


def retarget_assets(retargeter, src_mesh, dst_mesh, asset_paths, prefix, suffix,
                    target_path, include_referenced=True, overwrite=False):
    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.set_editor_properties(dict(
        assets_to_retarget=[unreal.EditorAssetLibrary.find_asset_data(p)
                            for p in asset_paths if exists(p)],
        source_mesh=src_mesh, target_mesh=dst_mesh, ik_retarget_asset=retargeter,
        prefix=prefix, suffix=suffix, target_path=target_path,
        include_referenced_assets=include_referenced,
        overwrite_existing_files=overwrite))
    missing = [p for p in asset_paths if not exists(p)]
    outputs = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    result = {"outputs": [], "missing_inputs": missing, "ok": True}
    dst_skel = dst_mesh.get_editor_property("skeleton") if dst_mesh else None
    for data in outputs or []:
        try:
            asset = data.get_asset()
        except Exception:
            try:
                asset = data
            except Exception:
                continue
        skel = safe(asset.get_skeleton) if hasattr(asset, "get_skeleton") \
            else safe(asset.get_editor_property, "skeleton")
        ok = (dst_skel is None) or (name_of(skel) == name_of(dst_skel)) or skel is None
        result["outputs"].append({"asset": asset.get_path_name(),
                                  "skeleton_match": bool(ok)})
        if not ok:
            result["ok"] = False
        save(asset)
    return result


# ------------------------------------------------------------------ sockets
def _make_socket(mesh, name, bone):
    """Create a socket-equivalent attachment point.

    Preferred: USkeletalMeshSocket via the DTMeshyEditorLibrary C++ helper
    (socket name properties are read-only in Python).
    Fallback:  a skeleton virtual bone pinned to BoneName — virtual bones are
    resolvable by name through the socket/bone query path, which is what
    DoesSocketExist / GetSocketTransform / attachment all use.
    """
    lib = getattr(unreal, "DTMeshyEditorLibrary", None)
    if lib is not None and hasattr(lib, "add_skeletal_socket"):
        try:
            return bool(lib.add_skeletal_socket(mesh, unreal.Name(name), unreal.Name(bone),
                                                unreal.Transform()))
        except Exception:
            pass
    skel = safe(mesh.get_editor_property, "skeleton")
    if skel and not isinstance(skel, str):
        for args in ((skel, bone, bone, name), (skel, unreal.Name(bone), unreal.Name(bone), unreal.Name(name))):
            try:
                unreal.AnimationLibrary.add_virtual_bone(*args)
                return True
            except Exception:
                continue
    return False


def ensure_sockets(mesh_or_path, extra=None):
    """Ensure the combat socket contract on a mesh. Returns report dict."""
    mesh = load(mesh_or_path) if isinstance(mesh_or_path, str) else mesh_or_path
    rep = {"mesh": name_of(mesh), "created": [], "already": [], "failed": {}}
    bones, _ = enumerate_bones(mesh)
    spec = dict(SOCKET_MAP)
    if extra:
        spec.update(extra)
    for sock, cands in spec.items():
        found = safe(mesh.find_socket, unreal.Name(sock))
        if found and not str(found).startswith("ERR") and found is not None:
            rep["already"].append(sock)
            continue
        bone = resolve_bone(bones, cands)
        if not bone:
            rep["failed"][sock] = "no matching bone"
            continue
        if _make_socket(mesh, sock, bone):
            rep["created"].append(sock + "@" + bone)
        else:
            rep["failed"][sock] = "socket construction failed (DuTravellerEditor module not built)"
    if rep["created"]:
        save(mesh)
    return rep


# ------------------------------------------------------------------ animbp adapt
ANIM_ASSET_PROPS = ("animation", "anim_sequence", "sequence", "blend_space", "asset")


def adapt_animbp(src_animbp=ENEMY_ANIMBP, dst_path=None, target_skeleton=None,
                 prefix="ABP_", suffix="", name=""):
    """Duplicate the enemy AnimBP onto the Meshy skeleton and remap every
    anim-asset reference in its graph to the retargeted equivalents.
    Batch retarget does not produce AnimBlueprints, so this is the primary path.
    Returns (bp_path, report)."""
    rep = {"src": src_animbp, "remapped": [], "missing_map": []}
    dst_path = dst_path or (OUT_ROOT + "/" + name + "/ABP_" + name + "_Enemy")
    skel = target_skeleton
    dup = unreal.load_asset(dst_path) if exists(dst_path) else \
        unreal.EditorAssetLibrary.duplicate_asset(src_animbp, dst_path)
    if not dup:
        rep["error"] = "duplicate failed"
        return dst_path, rep
    repair = unreal.DTStructRepairLibrary
    if skel:
        repair.assign_anim_skeleton(dst_path, skel)
    # build name map: <source asset short name> -> retargeted asset
    retargeted = {}
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    out_dir = dst_path.rsplit("/", 1)[0]
    for ad in reg.get_assets_by_path(out_dir, True):
        cls = str(ad.asset_class_path.asset_name)
        if cls not in ("AnimSequence", "AnimMontage", "BlendSpace", "BlendSpace1D"):
            continue
        pn = str(ad.package_name)
        base = short(pn)
        if base.startswith(prefix):
            retargeted[base[len(prefix):]] = pn
        retargeted[base] = pn

    # Preferred path: C++ remap helper (DuTravellerEditor module) which walks the
    # anim graph nodes natively. Source side of the map = every asset in the
    # project dirs whose short name matches a retargeted output.
    lib = getattr(unreal, "DTMeshyEditorLibrary", None)
    if lib is not None and hasattr(lib, "remap_anim_blueprint_animations"):
        name_to_target = {}
        for key, pn in retargeted.items():
            stripped = key
            if suffix and stripped.endswith(suffix):
                stripped = stripped[: -len(suffix)]
            for p in ("DT_", "ABP_", "FN_", "RTG_", "UEFN_"):
                if stripped.startswith(p):
                    stripped = stripped[len(p):]
            name_to_target[stripped] = pn
        src_map = {}
        for ad in reg.get_assets_by_path("/Game/Art/3D", True):
            cls = str(ad.asset_class_path.asset_name)
            if cls not in ("AnimSequence", "AnimMontage", "BlendSpace", "BlendSpace1D"):
                continue
            base = short(str(ad.package_name))
            if base in name_to_target:
                src_map[str(ad.package_name)] = name_to_target[base]
        try:
            n = lib.remap_anim_blueprint_animations(dst_path, src_map)
            rep["remap_count"] = int(n)
            rep["remap_method"] = "native"
            if n:
                unreal.DTStructRepairLibrary.force_compile_and_save(dst_path)
                rep["compile_saved"] = True
                rep["bp"] = dst_path
                return dst_path, rep
        except Exception as exc:
            rep["native_remap_err"] = str(exc)
    # walk anim graph nodes inside the duplicated bp and swap anim refs
    count = 0

    def _graph_nodes():
        bel = unreal.BlueprintEditorLibrary
        seen = set()
        graphs = []
        for prop in ("ubergraph_pages", "function_graphs", "animation_graph"):
            gs = safe(dup.get_editor_property, prop)
            if isinstance(gs, (list, tuple)):
                graphs.extend(gs)
        for name in ("AnimGraph", "Locomotion_1", "Idle/walk", "Jump", "Stunned",
                     "Getup", "GetStunned", "Burning", "Stun"):
            g = safe(bel.find_graph, dup, name)
            if g and not isinstance(g, str):
                graphs.append(g)
        for g in graphs:
            nid = str(g)
            if nid in seen:
                continue
            seen.add(nid)
            nodes = safe(g.get_editor_property, "nodes")
            if not isinstance(nodes, (list, tuple)):
                continue
            for n in nodes:
                yield n

    try:
        for node in _graph_nodes():
            targets = [node]
            sub = safe(node.get_editor_property, "node")
            if sub and not isinstance(sub, str):
                targets.append(sub)
            for t in targets:
                for prop in ANIM_ASSET_PROPS:
                    cur = safe(t.get_editor_property, prop)
                    if not cur or isinstance(cur, str):
                        continue
                    cur_name = short(cur.get_path_name())
                    for cand in (prefix + cur_name + suffix, prefix + cur_name,
                                 cur_name + suffix):
                        new_path = retargeted.get(cand) or retargeted.get(
                            (prefix + cand) if cand not in retargeted else cand)
                        if new_path and new_path != cur.get_path_name().split(".")[0]:
                            try:
                                t.set_editor_property(prop, load(new_path))
                                rep["remapped"].append(cur_name + " -> " + new_path)
                                count += 1
                            except Exception as e:
                                rep.setdefault("set_errors", {})[cur_name] = str(e)
                            break
                    else:
                        if "." not in cur_name and not any(
                                cur_name in n for n in rep["remapped"]):
                            rep["missing_map"].append(prop + ":" + cur_name)
    except Exception as exc:
        rep["walk_err"] = str(exc)
    rep["remap_count"] = count
    try:
        repair.refresh_blueprint_nodes(dst_path)
        ok = repair.force_compile_and_save(dst_path)
        rep["compile_saved"] = ok
    except Exception as exc:
        rep["compile_err"] = str(exc)
    rep["bp"] = dst_path
    return dst_path, rep


# ------------------------------------------------------------------ enemy bp
def create_enemy_bp(name, mesh, anim_class, parent_bp=ENEMY_PARENT_BP,
                    controller_bp=ENEMY_CONTROLLER_BP, router_map=None,
                    hit_reaction=None, capsule=None, walk_speed=250.0):
    """Create (or update) BP_<name>_Enemy as child of the existing enemy BP."""
    bp_path = ENEMY_BP_ROOT + "/BP_" + name + "_Enemy"
    parent = unreal.load_class(None, parent_bp + "." + short(parent_bp) + "_C")
    rep = {"bp": bp_path}
    if exists(bp_path):
        bp = load(bp_path)
        rep["reused"] = True
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        bp = tools().create_asset("BP_" + name + "_Enemy", ENEMY_BP_ROOT,
                                  unreal.Blueprint, factory)
        rep["reused"] = False
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    mesh_comp = cdo.get_editor_property("mesh")
    sk_mesh = load(mesh) if isinstance(mesh, str) else mesh
    mesh_comp.set_skeletal_mesh_asset(sk_mesh)
    if anim_class:
        anim_cls = anim_class
        if isinstance(anim_class, str):
            anim_cls = unreal.load_class(None, anim_class)
            if anim_cls is None and exists(anim_class):
                anim_cls = unreal.load_asset(anim_class).generated_class()
        if anim_cls:
            mesh_comp.set_anim_instance_class(anim_cls)
    # fit capsule + mesh offset from imported bounds when not provided
    bounds = safe(sk_mesh.get_imported_bounds)
    half = radius = None
    try:
        half = float(bounds.box_extent.z)
        radius = max(25.0, min(45.0, float(bounds.box_extent.y) * 1.35))
    except Exception:
        pass
    if capsule:
        half, radius = capsule
    if half:
        caps = cdo.get_editor_property("capsule_component")
        caps.set_editor_property("capsule_half_height", round(half, 1))
        caps.set_editor_property("capsule_radius", round(radius or 34.0, 1))
        mesh_comp.set_editor_property("relative_location", unreal.Vector(0, 0, -round(half, 1)))
    else:
        mesh_comp.set_editor_property("relative_location", unreal.Vector(0, 0, -88))
    mesh_comp.set_editor_property("relative_rotation", unreal.Rotator(pitch=0, yaw=-90, roll=0))
    mesh_comp.set_editor_property("visibility_based_anim_tick_option",
                                  unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
    cdo.set_editor_property("faction_id", unreal.Name("Enemy"))
    try:
        cdo.set_editor_property("faction_id", "Enemy")
    except Exception:
        pass
    if controller_bp:
        ctrl = unreal.load_class(None, controller_bp + "." + short(controller_bp) + "_C")
        if ctrl:
            cdo.set_editor_property("ai_controller_class", ctrl)
    cdo.set_editor_property("auto_possess_ai", unreal.AutoPossessAI.PLACED_IN_WORLD_OR_SPAWNED)
    cdo.set_editor_property("enemy_class", name)
    movement = cdo.get_editor_property("character_movement")
    movement.set_editor_property("max_walk_speed", float(walk_speed))
    movement.set_editor_property("use_controller_desired_rotation", True)
    movement.set_editor_property("orient_rotation_to_movement", False)
    router = cdo.get_editor_property("animation_router")
    if router_map:
        entries = {}
        for key, montage_path in router_map.items():
            row = unreal.DTCombatAnimation()
            row.set_editor_property("montage", load(montage_path))
            entries[key] = row
        router.set_editor_property("combat_animations", entries)
    if hit_reaction:
        hr = unreal.DTCombatAnimation()
        hr.set_editor_property("montage", load(hit_reaction))
        router.set_editor_property("hit_reaction", hr)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    rep["compile_status"] = str(safe(bp.get_editor_property, "status"))
    save(bp)
    rep["saved"] = True
    return rep


# ------------------------------------------------------------------ library scan
def animation_library_for_skeleton(skeleton_path, classes=("AnimSequence", "AnimMontage", "BlendSpace")):
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    out = []
    for ad in reg.get_assets_by_path("/Game", True):
        cls = str(ad.asset_class_path.asset_name)
        if cls not in classes:
            continue
        pn = str(ad.package_name)
        asset = unreal.load_asset(pn)
        if not asset:
            continue
        skel = safe(asset.get_skeleton) if hasattr(asset, "get_skeleton") \
            else safe(asset.get_editor_property, "skeleton")
        if name_of(skel) == name_of(skeleton_path):
            out.append(pn)
    return out


# ------------------------------------------------------------------ pipelines
def _prefixed_target_list(paths, prefix, suffix, target_path):
    return [target_path + "/" + prefix + short(p) + suffix for p in paths]


def run_pipeline(fbx=None, name=None, mesh_path=None, routes=("A", "C"),
                 retarget_library=True, create_enemy=True, adapt_animbp=True,
                 report_name=None, auto_chains=True, auto_align=True,
                 root_motion="generate", direction="source_to_meshy"):
    """End-to-end: import -> rig -> retargeters -> retarget -> AnimBP -> enemy BP."""
    rep = {"passed": False, "stages": {}, "errors": []}
    try:
        # ---- import / locate target mesh
        if mesh_path:
            mesh_path = norm_path(mesh_path)
            target_mesh = load(mesh_path)
            name = name or short(mesh_path).replace("SK_Meshy_", "").replace("SK_", "")
            rep["stages"]["import"] = {"mesh": mesh_path, "skipped": True}
        else:
            name = name or Path(str(fbx)).stem.replace(" ", "_")
            imp = import_meshy_fbx(fbx, name)
            rep["stages"]["import"] = imp
            if imp.get("requires_rigging"):
                rep["stages"]["import"]["status"] = "NOT_RIGGED"
                return rep
            target_mesh = load(imp["mesh"])
        rep["name"] = name
        analysis = analyze_skeletal_mesh(target_mesh)
        rep["stages"]["analysis"] = analysis
        if not analysis.get("rigged"):
            rep["stages"]["analysis"]["status"] = "NOT_RIGGED"
            return rep

        # ---- target IK rig
        dst_rig, dst_rep = ensure_ik_rig(target_mesh, auto=auto_chains)
        rep["stages"]["target_rig"] = dst_rep

        # ---- sources per route
        sources = {}
        if "A" in routes:
            src_mesh = load(UE4_MANNEQUIN_MESH)
            rig = load(IK_DT_LegacyUE4)
            if not rig:
                rig, rrep = ensure_ik_rig(src_mesh, rig_name="IK_DT_LegacyUE4", auto=auto_chains)
                rep["stages"]["src_rig_A"] = rrep
            sources["A"] = (src_mesh, rig, "Enemy", "DT_", "_Meshy_" + name)
        if "C" in routes:
            src_mesh = load(UEFN_MESH)
            rig = load(IK_DT_UEFN)
            if not rig:
                rig, rrep = ensure_ik_rig(src_mesh, rig_name="IK_DT_UEFN", auto=auto_chains)
                rep["stages"]["src_rig_C"] = rrep
            sources["C"] = (src_mesh, rig, "FNMannequin", "FN_", "_Meshy_" + name)
        if "B" in routes:
            src_mesh = None
            for cand in ROUTE_B_MANNY_CANDIDATES:
                if exists(cand):
                    src_mesh = load(cand)
                    break
            rig = load(ROUTE_B_MANNY_RIG) if exists(ROUTE_B_MANNY_RIG) else None
            if not src_mesh:
                # fall back to the in-project UE5-mannequin-named serpent skeleton
                serpent = load(SERPENT_MESH)
                if serpent:
                    bones, _ = enumerate_bones(serpent)
                    if detect_convention(bones) == "ue":
                        src_mesh = serpent
                        rig, rrep = ensure_ik_rig(serpent, rig_name="IK_Meshy_UE5Mannequin_Serpent",
                                                  auto=auto_chains)
                        rep["stages"]["src_rig_B"] = dict(rrep, fallback="serpent_ue5_skeleton")
            if src_mesh:
                if not rig:
                    rig, rrep = ensure_ik_rig(src_mesh, rig_name="IK_DT_UE5Mannequin", auto=auto_chains)
                    rep["stages"]["src_rig_B"] = rrep
                sources["B"] = (src_mesh, rig, "Manny", "MM_", "_Meshy_" + name)
            else:
                rep["stages"]["route_B"] = {"unavailable": "no UE5 Manny/Quinn skeleton or mesh found"}

        rep["stages"]["retargeters"] = {}
        for route, (src_mesh, src_rig, tag, prefix, suffix) in sources.items():
            rtg, rrep = ensure_retargeter(src_rig, dst_rig, src_mesh, target_mesh,
                                          auto_map=auto_chains, auto_align=auto_align,
                                          root_motion=root_motion)
            rep["stages"]["retargeters"][route] = rrep

        # ---- batch retarget
        if retarget_library:
            rep["stages"]["retargets"] = {}
            for route, (src_mesh, src_rig, tag, prefix, suffix) in sources.items():
                if route == "A":
                    lib = sorted({p for p in ROUTE_A_LIBRARY + ROUTE_A_ANIMBP_SEQUENCES
                                  if exists(p)})
                elif route == "C":
                    lib = [p for p in ROUTE_C_LIBRARY if exists(p)]
                else:
                    lib = []
                    for cand in ROUTE_B_MANNY_CANDIDATES:
                        d = cand.rsplit("/", 1)[0]
                        lib += []  # populated below via registry when plugin mounted
                    reg = unreal.AssetRegistryHelpers.get_asset_registry()
                    for ad in reg.get_assets_by_path(ROUTE_B_MANNY_ANIM_DIR, True):
                        cls = str(ad.asset_class_path.asset_name)
                        if cls in ("AnimSequence", "AnimMontage"):
                            lib.append(str(ad.package_name))
                    lib = sorted(set(lib))
                if not lib:
                    rep["stages"]["retargets"][route] = {"skipped": "no source assets"}
                    continue
                rtg = load(rep["stages"]["retargeters"][route]["retargeter"])
                out = retarget_assets(rtg, src_mesh, target_mesh, lib, prefix, suffix,
                                      OUT_ROOT + "/" + name + "/Route" + route)
                rep["stages"]["retargets"][route] = out

        # ---- adapt enemy AnimBP (batch op can't retarget anim blueprints)
        if adapt_animbp and "A" in rep["stages"].get("retargeters", {}):
            bp_path, arep = adapt_animbp(
                ENEMY_ANIMBP,
                OUT_ROOT + "/" + name + "/ABP_" + name + "_Enemy",
                analysis["skeleton"],
                prefix="DT_", suffix="_Meshy_" + name, name=name)
            rep["stages"]["animbp"] = arep
            rep["animbp_path"] = bp_path

        # ---- sockets
        rep["stages"]["sockets"] = ensure_sockets(target_mesh)

        # ---- enemy blueprint
        if create_enemy:
            rt = rep["stages"].get("retargets", {}).get("A", {})
            outputs = {short(o["asset"]): o["asset"] for o in rt.get("outputs", [])}
            def find(*frags):
                for oname, opath in outputs.items():
                    if all(f in oname for f in frags):
                        return opath
                return None
            router_map = {}
            slash = find("Sword_And_Shield_Slash", "Montage") or find("Axe_DownSlash")
            cast = find("1H_Cast", "Montage") or find("1HCast")
            hit = find("ReactHitFont") or find("ReactHit") or find("React")
            death = find("Falling_Back_Death")
            if slash:
                router_map["AI.CombatTest.Slash"] = slash
            if cast:
                router_map["AI.CombatTest.FireBolt"] = cast
            if death:
                router_map["AI.Death"] = death
            anim_bp_path = rep.get("animbp_path")
            rep["stages"]["enemy_bp"] = create_enemy_bp(
                name, target_mesh, anim_bp_path if exists(anim_bp_path or "") else None,
                router_map=router_map, hit_reaction=hit)
            rep["enemy_bp"] = ENEMY_BP_ROOT + "/BP_" + name + "_Enemy"

        rep["passed"] = True
    except Exception:
        rep["errors"].append(traceback.format_exc())
    return rep


def validate(name=None, mesh_path=None, enemy_bp=None, anim_bp=None, report=None):
    """Validation pass over generated content."""
    rep = {"checks": [], "passed": True, "errors": []}

    def check(label, ok, detail=""):
        rep["checks"].append({"check": label, "ok": bool(ok), "detail": str(detail)[:400]})
        if not ok:
            rep["passed"] = False

    if report:
        rep["pipeline"] = report
    if mesh_path:
        mesh = load(mesh_path)
        check("mesh loads", mesh is not None, mesh_path)
        if mesh:
            a = analyze_skeletal_mesh(mesh)
            check("mesh is skinned", a.get("rigged"), a.get("bone_count"))
            check("humanoid chains", a.get("humanoid"), a.get("canonical_missing"))
            skel = a.get("skeleton")
            for sock in ("hand_r", "hand_l", "weapon_r", "head"):
                found = safe(mesh.find_socket, unreal.Name(sock))
                check("socket " + sock, found is not None and not str(found).startswith("ERR"),
                      str(found)[:80])
            rep["analysis"] = a
    if enemy_bp and exists(enemy_bp):
        bp = load(enemy_bp)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        status = safe(bp.get_editor_property, "status")
        check("enemy bp compiles", str(status) != "<BlueprintStatus.BS_ERROR: 2>", status)
        cdo = unreal.get_default_object(bp.generated_class())
        mesh = cdo.get_editor_property("mesh")
        sm = safe(mesh.get_editor_property, "skeletal_mesh_asset")
        check("enemy mesh set", sm is not None, name_of(sm))
        if sm:
            skel = safe(sm.get_editor_property, "skeleton")
            router = safe(cdo.get_editor_property, "animation_router")
            if router and not isinstance(router, str):
                for k, v in router.get_editor_property("combat_animations").items():
                    m = v.get_editor_property("montage")
                    if not m:
                        continue
                    msk = safe(m.get_skeleton)
                    check("montage skeleton " + str(k), name_of(msk) == name_of(skel),
                          name_of(m) + " -> " + str(name_of(msk)))
                    def _notify_count(mont):
                        if not mont:
                            return -1
                        desc = safe(unreal.DTStructRepairLibrary.describe_montage_notifies,
                                    mont.get_path_name())
                        if isinstance(desc, str):
                            return -1
                        try:
                            return len(desc)
                        except Exception:
                            return -1
                    n_count = _notify_count(m)
                    src_name = m.get_name()
                    if src_name.startswith("DT_"):
                        src_name = src_name[3:]
                    if src_name.endswith("_Meshy_" + name):
                        src_name = src_name[: -len("_Meshy_" + name)]
                    src_count = -2
                    for ad in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path(
                            "/Game/Art/3D", True):
                        if short(str(ad.package_name)) == src_name:
                            src_count = _notify_count(load(str(ad.package_name)))
                            break
                    ok = n_count == src_count if src_count != -2 else n_count >= 0
                    check("montage notifies " + str(k), ok,
                          "retargeted=" + str(n_count) + " source=" + str(src_count))
        check("enemy anim class", safe(lambda: mesh.get_editor_property("anim_class")) is not None,
              str(safe(lambda: name_of(mesh.get_editor_property("anim_class")))))
        check("enemy controller", safe(cdo.get_editor_property, "ai_controller_class") is not None,
              str(safe(lambda: name_of(cdo.get_editor_property("ai_controller_class")))))
    if anim_bp and exists(anim_bp):
        bp = load(anim_bp)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        check("anim bp compiles", str(safe(bp.get_editor_property, "status")) != "<BlueprintStatus.BS_ERROR: 2>",
              safe(bp.get_editor_property, "status"))
    return rep


def write_report(report, name="MeshyRetarget"):
    out = Path(unreal.Paths.project_saved_dir()) / "Automation" / (name + ".json")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(report, indent=2, default=str), encoding="utf-8")
    return str(out)


# ------------------------------------------------------------------ EUW bridge
def euw_run(json_args):
    """Invoked from the C++ EUW via IPythonScriptPlugin::ExecPythonCommand."""
    try:
        args = json.loads(json_args) if isinstance(json_args, str) else dict(json_args)
    except Exception:
        args = {}
    action = args.get("action", "analyze")
    rep = {"action": action}
    try:
        if action == "analyze":
            rep["result"] = analyze_skeletal_mesh(load(args["mesh"]))
        elif action == "import":
            rep["result"] = import_meshy_fbx(args["fbx"], args.get("name"))
        elif action == "build_rigs":
            mesh = load(args["mesh"])
            dst, r1 = ensure_ik_rig(mesh, auto=args.get("auto_chains", True))
            rep["result"] = {"target": r1}
        elif action == "pipeline":
            rep["result"] = run_pipeline(
                mesh_path=args.get("mesh"), fbx=args.get("fbx"), name=args.get("name"),
                routes=tuple(args.get("routes", ("A", "C"))),
                auto_chains=args.get("auto_chains", True),
                auto_align=args.get("auto_align", True),
                root_motion=args.get("root_motion", "generate"),
                direction=args.get("direction", "source_to_meshy"))
        elif action == "validate":
            rep["result"] = validate(mesh_path=args.get("mesh"),
                                     enemy_bp=args.get("enemy_bp"),
                                     anim_bp=args.get("anim_bp"))
        else:
            rep["error"] = "unknown action " + action
    except Exception:
        rep["exception"] = traceback.format_exc()
    write_report(rep, "MeshyRetargetUI")
    return rep
