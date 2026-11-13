"""Staged smoke test: analyze -> IK rig -> retargeter -> single montage retarget."""
import os
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import unreal  # noqa: E402
import MeshyRetargetCore as M  # noqa: E402

report = {"passed": False, "stages": {}}
try:
    mesh = unreal.load_asset(
        "/Game/DuTraveller/Characters/Imported/Meshy/AzureLichKing/SK_Meshy_AzureLichKing")
    report["stages"]["analysis"] = M.analyze_skeletal_mesh(mesh)

    rig, rrep = M.ensure_ik_rig(mesh, auto=True)
    report["stages"]["rig"] = rrep

    src_mesh = unreal.load_asset(M.UE4_MANNEQUIN_MESH)
    src_rig = unreal.load_asset(M.IK_DT_LegacyUE4)
    rtg, trep = M.ensure_retargeter(src_rig, rig, src_mesh, mesh,
                                  name="RTG_Enemy_To_Meshy_AzureLichKing")
    report["stages"]["retargeter"] = trep

    out = M.retarget_assets(rtg, src_mesh, mesh,
                            ["/Game/Art/3D/Ai/Enemy/Anims/AMAI_ReactHitFont"],
                            "DT_", "_Meshy_AzureLichKing",
                            "/Game/DuTraveller/Animations/Retargeted/AzureLichKing/RouteA")
    report["stages"]["retarget"] = out
    report["passed"] = bool(out.get("outputs")) and out.get("ok", False)
except Exception:
    import traceback
    report["exception"] = traceback.format_exc()

out = M.write_report(report, "MeshySmokeTest")
unreal.log("MESHY_SMOKE " + str(report["passed"]))
unreal.SystemLibrary.quit_editor()
