"""Recover the installed Epic UE4 mannequin into absent historical package paths."""
import json
from pathlib import Path
import re
import unreal

report = {"passed": False, "source": "Epic installed AnimToTexture UE4 mannequin", "retargeted_animations": False}
try:
    source_root = Path(unreal.Paths.engine_plugins_dir()) / "Experimental/AnimToTexture/Content"
    prefix = "/AnimToTexture/Characters/Mannequin/"
    pending = [prefix + "Meshes/SK_Mannequin", prefix + "Meshes/SKM_Mannequin"]
    packages = {}
    while pending:
        package = pending.pop()
        if package in packages:
            continue
        if not package.startswith(prefix):
            raise RuntimeError("Unexpected dependency outside the recovery subtree: " + package)
        source_file = source_root / (package[len("/AnimToTexture/"):] + ".uasset")
        if not source_file.exists():
            raise RuntimeError("Missing source dependency: " + str(source_file))
        target = "/Game/Characters/Mannequin_UE4/" + package[len(prefix):]
        if package.endswith("Meshes/SK_Mannequin"):
            target = "/Game/Characters/Mannequin_UE4/Meshes/SK_Mannequin_Skeleton"
        elif package.endswith("Meshes/SKM_Mannequin"):
            target = "/Game/Characters/Mannequin_UE4/Meshes/SK_Mannequin"
        target_file = Path(unreal.Paths.project_content_dir()) / (target[len("/Game/"):] + ".uasset")
        if target_file.exists():
            raise RuntimeError("Refusing to overwrite existing package: " + str(target_file))
        packages[package] = target
        text = source_file.read_bytes().decode("utf-8", errors="ignore")
        pending.extend(value for value in set(re.findall(r"/AnimToTexture/[A-Za-z0-9_/-]+", text)) if value not in packages)
    report["source_validation"] = unreal.DTAnimationMigrationLibrary.inspect_legacy_mannequin()
    if not report["source_validation"].startswith("OK "):
        raise RuntimeError(report["source_validation"])
    report["package_map"] = packages
    if not unreal.DTAnimationMigrationLibrary.copy_legacy_mannequin(packages):
        raise RuntimeError("Unreal refused the guarded dependency copy")
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(["/Game/Characters/Mannequin_UE4"], True)
    report["saved_packages"] = []
    for target in packages.values():
        copied = unreal.load_asset(target)
        if not copied or not unreal.EditorAssetLibrary.save_loaded_asset(copied, False):
            raise RuntimeError("Failed to save recovered package: " + target)
        target_file = Path(unreal.Paths.project_content_dir()) / (target[len("/Game/"):] + ".uasset")
        if not target_file.exists():
            raise RuntimeError("Recovered package was not persisted: " + str(target_file))
        report["saved_packages"].append(target)
    skeleton = unreal.load_asset("/Game/Characters/Mannequin_UE4/Meshes/SK_Mannequin_Skeleton")
    mesh = unreal.load_asset("/Game/Characters/Mannequin_UE4/Meshes/SK_Mannequin")
    if not isinstance(skeleton, unreal.Skeleton) or not isinstance(mesh, unreal.SkeletalMesh):
        raise RuntimeError("Recovered asset types do not match")
    report["restored_skeleton"] = skeleton.get_path_name()
    report["restored_mesh"] = mesh.get_path_name()
    report["checked_montages"] = []
    for path in ("/Game/Art/3D/Animations/Warrior/AM_Warrior_Axe_DownSlash",
                 "/Game/Art/3D/Player/Animations/Anims/Retargeted/Sword_And_Shield_Slash_mixamo_com_Montage"):
        montage = unreal.load_asset(path)
        report["checked_montages"].append({"asset": path, "skeleton_resolves": montage.get_editor_property("skeleton") == skeleton})
    report["passed"] = all(row["skeleton_resolves"] for row in report["checked_montages"])
finally:
    output = Path(unreal.Paths.project_saved_dir()) / "Automation/LegacyMannequinRecovery.json"
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("DT_MANNEQUIN_RECOVERY " + json.dumps(report))
    unreal.SystemLibrary.quit_editor()
