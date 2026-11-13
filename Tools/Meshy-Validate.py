"""Validation pass over a generated Meshy enemy. Headless runner.

Env: MESHY_NAME (required), MESHY_MESH, MESHY_ENEMY_BP, MESHY_ANIMBP
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import unreal  # noqa: E402
import MeshyRetargetCore as M  # noqa: E402

name = os.environ.get("MESHY_NAME", "AzureLichKing")
mesh = os.environ.get("MESHY_MESH",
                      "/Game/DuTraveller/Characters/Imported/Meshy/" + name +
                      "/SK_Meshy_" + name)
enemy_bp = os.environ.get("MESHY_ENEMY_BP", "/Game/Blueprints/AI/BP_" + name + "_Enemy")
anim_bp = os.environ.get("MESHY_ANIMBP") or None

report = M.validate(name=name, mesh_path=mesh, enemy_bp=enemy_bp, anim_bp=anim_bp)
out = M.write_report(report, "MeshyValidate_" + name)
unreal.log("MESHY_VALIDATE " + str(report["passed"]))
unreal.SystemLibrary.quit_editor()
