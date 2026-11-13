import unreal

unreal.log("has find_enum: {0}".format(hasattr(unreal, "find_enum")))
unreal.log("has load_struct: {0}".format(hasattr(unreal, "load_struct")))
unreal.log("has FQuestDetails type: {0}".format(hasattr(unreal, "FQuestDetails")))

try:
    t = getattr(unreal, "FQuestDetails", None)
    unreal.log("FQuestDetails type: {0}".format(t))
    if t is not None and hasattr(t, "static_struct"):
        unreal.log("static_struct: {0}".format(t.static_struct()))
except Exception as exc:
    unreal.log_error("static_struct probe: {0}".format(exc))

try:
    pkg = unreal.load_package("/Script/DuTraveller")
    unreal.log("package: {0}".format(pkg))
    unreal.log("find in pkg: {0}".format(unreal.find_object(pkg, "FQuestDetails")))
except Exception as exc:
    unreal.log_error("pkg probe: {0}".format(exc))

try:
    unreal.log("load_object raw name: {0}".format(unreal.load_object(None, "/Script/DuTraveller.FQuestDetails")))
except Exception as exc:
    unreal.log_error("load_object: {0}".format(exc))

unreal.SystemLibrary.quit_editor()
