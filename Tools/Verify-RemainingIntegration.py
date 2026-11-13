"""Read-only verification of the remaining integration items.
Run: UnrealEditor-Cmd.exe DT.uproject -run=pythonscript -script=<this> -unattended -nop4 -nosplash -stdout -FullStdOutLogOutput
Never saves assets.
"""
import json
from pathlib import Path
import unreal

out = {}


def safe(fn, *a):
    try:
        return fn(*a)
    except Exception as exc:  # noqa: BLE001
        return "ERR:" + str(exc)[:160]


def path_of(obj):
    try:
        return obj.get_path_name() if obj else None
    except Exception:
        return str(obj)


def prop(obj, name):
    return safe(obj.get_editor_property, name)


def skel_of(mesh_asset):
    if not mesh_asset or isinstance(mesh_asset, str):
        return None
    return str(safe(lambda: mesh_asset.skeleton.get_path_name()))


# ---- Quest giver Blueprint member variables and CDO values -----------------
qg_bp = unreal.load_asset("/Game/Blueprints/Quest/BP_QuestGiver")
if qg_bp:
    names = [n for n in unreal.BlueprintEditorLibrary.list_member_variable_names(qg_bp) if "/" not in n]
    cdo = unreal.get_default_object(qg_bp.generated_class())
    values = {}
    for n in names:
        values[n] = str(safe(lambda n=n: prop(cdo, n)))
    out["quest_giver"] = {
        "status": str(prop(qg_bp, "status")),
        "member_vars": names,
        "values": values,
        "parent": str(safe(lambda: unreal.BlueprintEditorLibrary.get_blueprint_parent_class(qg_bp).get_path_name())),
    }

# ---- NPC Blueprints: mesh/anim assignment -----------------------------------
for bp_path in ["/Game/Blueprints/AI/NPC/BP_NPC_Trainer",
                "/Game/Blueprints/AI/BP_RPGAICharacter",
                "/Game/Blueprints/AI/BP_AIRangedCharacter",
                "/Game/Blueprints/Quest/BP_QuestGiver",
                "/Game/Blueprints/Player/BP_RPGCharacter",
                "/Game/Blueprints/Characters/BP_PlayerCharacter"]:
    if not unreal.EditorAssetLibrary.does_asset_exist(bp_path):
        continue
    bp = unreal.load_asset(bp_path)
    cdo = unreal.get_default_object(bp.generated_class())
    mesh = prop(cdo, "mesh")
    asset = prop(mesh, "skeletal_mesh_asset")
    out.setdefault("blueprints", {})[bp_path] = {
        "status": str(prop(bp, "status")),
        "parent": str(safe(lambda: unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp).get_path_name())),
        "mesh_asset": path_of(asset),
        "mesh_skeleton": skel_of(asset),
        "anim_class": path_of(prop(mesh, "anim_class")),
        "anim_mode": str(prop(mesh, "animation_mode")),
    }

# ---- Candidate NPC meshes: skeletons ----------------------------------------
for mp in ["/Game/Art/3D/Ai/NPCS/Trainer/Trainer",
           "/Game/Art/3D/Ai/NPCS/Captain/Captain",
           "/Game/Art/3D/Ai/NPCS/Keeper/Keeper",
           "/Game/Art/3D/Ai/NPCS/Knight/Knight",
           "/Game/Art/3D/Ai/NPCS/Traider/Trader",
           "/Game/Art/3D/Ai/NPCS/Wanderer/Wanderer",
           "/Game/Characters/Mannequin_UE4/Meshes/SK_Mannequin",
           "/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin",
           "/Game/Art/3D/Ai/Enemy/Elf_Melee_UE4",
           "/Game/Art/3D/Ai/Enemy/Elf_Mage_UE4",
           "/Game/Art/3D/Ai/Enemy/GolemWithBones"]:
    a = unreal.load_asset(mp)
    if a:
        out.setdefault("candidate_meshes", {})[mp] = skel_of(a)

# ---- AnimBP targets ----------------------------------------------------------
for ap in ["/Game/Art/3D/Player/Animations/Anims/AnimBP_RPGAICharacter",
           "/Game/Art/3D/Player/Animations/Anims/AnimBP_RPGCharacter",
           "/Game/Blueprints/Animations/ABP_SandboxCharacter",
           "/Game/Characters/UEFN_Mannequin/Rigs/ABP_UEFN_Mannequin_PostProcess"]:
    bp = unreal.load_asset(ap)
    if not bp:
        continue
    gen = bp.generated_class()
    info = {
        "status": str(prop(bp, "status")),
        "target_skeleton": str(safe(lambda: bp.get_editor_property("target_skeleton").get_path_name())),
        "parent_class": str(safe(lambda: gen.get_super_class().get_path_name())),
    }
    out.setdefault("anim_bps", {})[ap] = info

# ---- Tutorial placed actors via editor actor subsystem -----------------------
actors_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
worlds = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = worlds.get_editor_world()
if not world or world.get_name() != "Tutorial":
    unreal.EditorLoadingAndSavingUtils.load_map("/Game/Levels/Tutorial")
    world = worlds.get_editor_world()
placed = []
for actor in actors_sub.get_all_level_actors():
    if not isinstance(actor, unreal.Character):
        continue
    mesh = prop(actor, "mesh")
    asset = prop(mesh, "skeletal_mesh_asset")
    placed.append({
        "actor": actor.get_actor_label(),
        "class": path_of(actor.get_class()),
        "mesh_asset": path_of(asset),
        "mesh_skeleton": skel_of(asset),
        "anim_class": path_of(prop(mesh, "anim_class")),
        "hidden": str(prop(actor, "actor_hidden_in_game")),
        "auto_possess": str(prop(actor, "auto_possess_player")),
    })
out["placed_characters"] = placed

# ---- Mount Blueprints + input ------------------------------------------------
mount_cls = unreal.load_class(None, "/Script/DuTraveller.RPGMountCharacter")
mounts = []
mount_prefix = "/Script/DuTraveller.RPGMountCharacter"
for actor in actors_sub.get_all_level_actors():
    if not isinstance(actor, unreal.Character):
        continue
    cls = actor.get_class()
    while cls:
        if cls.get_path_name() == mount_prefix:
            mounts.append({"actor": actor.get_actor_label(), "class": path_of(actor.get_class())})
            break
        cls = cls.get_super_class()
out["placed_mounts"] = mounts

imc = unreal.load_asset("/Game/Input/IMC_GDCMotionMatching")
if imc:
    keys = {}
    for m in prop(imc, "default_key_mappings") or []:
        a = prop(m, "action")
        keys.setdefault(path_of(a), []).append(str(prop(m, "key")))
    out["imc_mappings"] = keys

report = Path(unreal.Paths.project_saved_dir()) / "Automation" / "RemainingIntegration.json"
report.parent.mkdir(parents=True, exist_ok=True)
report.write_text(json.dumps(out, indent=1, default=str), encoding="utf-8")
unreal.log("VERIFY " + json.dumps(out, default=str))
unreal.SystemLibrary.quit_editor()
