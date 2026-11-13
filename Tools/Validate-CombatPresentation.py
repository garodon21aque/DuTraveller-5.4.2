import json,runpy
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
runpy.run_path(str(root/"Tools/Repair-CombatPresentation.py"))
if not json.loads((root/"Saved/Automation/CombatPresentationRepair.json").read_text())["passed"]: raise RuntimeError("Presentation repair failed")
runpy.run_path(str(root/"Tools/Run-CombatPresentationPIE.py"),init_globals={"RUN_REGRESSION_AFTER":True})
