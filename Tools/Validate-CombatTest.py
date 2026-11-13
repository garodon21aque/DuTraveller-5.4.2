"""Save the fixture, run rendered PIE, then run restoration regressions.

Launch with -TestExit="Automation Test Queue Empty" and -ReportExportPath.
"""
import json
import runpy
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
runpy.run_path(str(root / "Tools/Create-CombatTest.py"))
setup = json.loads((root / "Saved/Automation/CombatTestSetup.json").read_text())
if not setup["passed"]:
    raise RuntimeError("Combat fixture setup failed; see CombatTestSetup.json")
runpy.run_path(str(root / "Tools/Run-CombatEncounterPIE.py"),init_globals={"RUN_REGRESSION_AFTER":True})
