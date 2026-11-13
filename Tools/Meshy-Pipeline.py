"""Meshy end-to-end pipeline runner (headless).

Usage via Run-PythonScript.cmd or directly:
  UnrealEditor-Cmd.exe <proj> -run=pythonscript -script=".../Meshy-Pipeline.py" ...

Environment overrides:
  MESHY_FBX    absolute path to a rigged FBX (default: Azure Lich King 0001)
  MESHY_MESH   existing skeletal mesh path (skips import)
  MESHY_NAME   character short name (default: AzureLichKing)
  MESHY_ROUTES comma list: A,B,C (default: A,C)
"""
import json
import os
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import unreal  # noqa: E402
import MeshyRetargetCore as M  # noqa: E402

root = Path(unreal.Paths.project_dir())
fbx = os.environ.get("MESHY_FBX", str(root / "Content/MeshyImports/Azure_Lich_King_Import0001.fbx"))
mesh = os.environ.get("MESHY_MESH") or None
name = os.environ.get("MESHY_NAME") or None
routes = tuple(s.strip() for s in os.environ.get("MESHY_ROUTES", "A,C").split(",") if s.strip())

report = M.run_pipeline(fbx=fbx, name=name, mesh_path=mesh, routes=routes,
                        retarget_library=True, create_enemy=True, adapt_animbp=True)
out = M.write_report(report, "MeshyPipeline_" + (report.get("name") or "run"))
unreal.log("MESHY_PIPELINE " + json.dumps({"passed": report.get("passed"),
                                          "errors": report.get("errors")}, default=str))
unreal.SystemLibrary.quit_editor()
