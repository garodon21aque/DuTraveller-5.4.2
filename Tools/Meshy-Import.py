"""Import + analyze one Meshy FBX without retargeting. Headless runner."""
import os
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import unreal  # noqa: E402
import MeshyRetargetCore as M  # noqa: E402

root = Path(unreal.Paths.project_dir())
fbx = os.environ.get("MESHY_FBX", str(root / "Content/MeshyImports/Azure_Lich_King_Import0001.fbx"))
name = os.environ.get("MESHY_NAME")

report = {"passed": False}
try:
    result = M.import_meshy_fbx(fbx, name or Path(fbx).stem.replace(" ", "_"))
    report["import"] = result
    report["passed"] = bool(result.get("mesh"))
except Exception:
    import traceback
    report["exception"] = traceback.format_exc()

out = M.write_report(report, "MeshyImport")
unreal.log("MESHY_IMPORT " + str(report["passed"]))
unreal.SystemLibrary.quit_editor()
