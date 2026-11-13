import unreal

names = [n for n in dir(unreal) if "ismet" in n or "tructure" in n or "ield" in n]
unreal.log("names: {0}".format(names))

# Can AssetTools create a UDS?
tools = unreal.AssetToolsHelpers.get_asset_tools()
try:
    asset = tools.create_asset("TempStruct", "/Game/_Temp", unreal.UserDefinedStruct, None)
    unreal.log("created: {0}".format(asset))
    if asset:
        # inspect for variable-related editor properties
        for prop in ("EditorData", "NewVariables", "Status", "Guid"):
            try:
                v = asset.get_editor_property(prop)
                unreal.log("  prop {0} = {1}".format(prop, v))
            except Exception as e:
                unreal.log("  prop {0} failed".format(prop))
except Exception as e:
    unreal.log("create failed: {0}".format(e))
