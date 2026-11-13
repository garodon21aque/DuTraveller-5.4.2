"""Fixup pass: adapt AnimBP (duplicate+reskeleton+remap), rewire enemy anim class,
retry sockets, then validate. Idempotent."""
import os
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import unreal  # noqa: E402
import MeshyRetargetCore as M  # noqa: E402

NAME = os.environ.get("MESHY_NAME", "AzureLichKing")
MESH = "/Game/DuTraveller/Characters/Imported/Meshy/" + NAME + "/SK_Meshy_" + NAME
report = {"passed": False, "stages": {}}

try:
    mesh = M.load(MESH)
    analysis = M.analyze_skeletal_mesh(mesh)
    report["analysis_skel"] = analysis.get("skeleton")

    bp_path, arep = M.adapt_animbp(
        M.ENEMY_ANIMBP,
        M.OUT_ROOT + "/" + NAME + "/ABP_" + NAME + "_Enemy",
        analysis["skeleton"],
        prefix="DT_", suffix="_Meshy_" + NAME, name=NAME)
    report["stages"]["animbp"] = arep

    # rewire anim class on the existing enemy BP
    enemy = M.load(M.ENEMY_BP_ROOT + "/BP_" + NAME + "_Enemy")
    if enemy:
        cdo = unreal.get_default_object(enemy.generated_class())
        anim_cls = unreal.load_class(None, bp_path + "." + M.short(bp_path) + "_C")
        cdo.get_editor_property("mesh").set_anim_instance_class(anim_cls)
        unreal.BlueprintEditorLibrary.compile_blueprint(enemy)
        M.save(enemy)
        report["stages"]["enemy_anim_class"] = str(anim_cls)

    report["stages"]["sockets"] = M.ensure_sockets(mesh)
    report["validation"] = M.validate(
        name=NAME, mesh_path=MESH,
        enemy_bp=M.ENEMY_BP_ROOT + "/BP_" + NAME + "_Enemy", anim_bp=bp_path)
    report["passed"] = report["validation"]["passed"]
except Exception:
    import traceback
    report["exception"] = traceback.format_exc()

M.write_report(report, "MeshyFixup_" + NAME)
unreal.log("MESHY_FIXUP " + str(report["passed"]))
unreal.SystemLibrary.quit_editor()
