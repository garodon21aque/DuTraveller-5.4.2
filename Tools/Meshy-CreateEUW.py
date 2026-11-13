"""Create EUW_DuTraveller_CharacterRetargeter parented to the native widget class.

Requires the DuTravellerEditor module to be built (UDuTravellerCharacterRetargeterWidget).
If the class is unavailable, creates the EUW anyway (base class) and reports it.
"""
import json
import os
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import unreal  # noqa: E402
import MeshyRetargetCore as M  # noqa: E402

EUW_PATH = M.EDITOR_ROOT + "/EUW_DuTraveller_CharacterRetargeter"
report = {"passed": False}

try:
    euw = M.load(EUW_PATH)
    created = False
    if not euw:
        factory = unreal.EditorUtilityWidgetBlueprintFactory()
        euw = M.tools().create_asset("EUW_DuTraveller_CharacterRetargeter", M.EDITOR_ROOT,
                                     unreal.EditorUtilityWidgetBlueprint, factory)
        created = True
    report["created"] = created
    report["euw"] = euw.get_path_name() if euw else None

    widget_cls = unreal.load_class(
        None, "/Script/DuTravellerEditor.DuTravellerCharacterRetargeterWidget")
    report["native_class_found"] = widget_cls is not None
    if euw and widget_cls:
        unreal.BlueprintEditorLibrary.reparent_blueprint(euw, widget_cls)
        unreal.BlueprintEditorLibrary.compile_blueprint(euw)
        M.save(euw)
        report["reparented"] = True
        report["status"] = str(unreal.EditorAssetLibrary.load_asset(EUW_PATH)
                               .get_editor_property("status")) if M.load(EUW_PATH) else None
    report["passed"] = report["euw"] is not None
except Exception:
    import traceback
    report["exception"] = traceback.format_exc()

M.write_report(report, "MeshyCreateEUW")
unreal.log("MESHY_EUW " + json.dumps(report, default=str))
unreal.SystemLibrary.quit_editor()
