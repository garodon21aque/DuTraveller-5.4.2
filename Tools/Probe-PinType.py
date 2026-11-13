import unreal

unreal.log("TerminalType? {0}".format(hasattr(unreal, "EdGraphTerminalType")))
names = [n for n in dir(unreal) if "Terminal" in n or "PinContainer" in n]
unreal.log("names: {0}".format(names))

# Constructor kwargs attempt
try:
    t = unreal.EdGraphPinType(pin_type=None)
    unreal.log("ctor kwarg pin_type accepted")
except Exception as e:
    unreal.log("ctor kwarg failed: {0}".format(e))

# export_text of an existing variable's pin type
bp = unreal.load_asset("/Game/UI/HUD/WBP_HUD")
if bp:
    try:
        vnames = unreal.BlueprintEditorLibrary.list_member_variable_names(bp)
        unreal.log("WBP_HUD vars: {0}".format(vnames))
        for n in vnames:
            pt = unreal.BlueprintEditorLibrary.get_member_variable_type(bp, n)
            unreal.log("  {0}: {1} export={2}".format(n, pt, pt.export_text() if hasattr(pt, "export_text") else "?"))
    except Exception as e:
        unreal.log("var list failed: {0}".format(e))
