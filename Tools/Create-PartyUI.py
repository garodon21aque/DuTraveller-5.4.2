import unreal

path = "/Game/UI/Party/WBP_PartyMenu"
bp = unreal.load_asset(path)
if not bp:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.load_class(None, "/Script/DuTraveller.RPGPartyWidget"))
    bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset("WBP_PartyMenu", "/Game/UI/Party", unreal.WidgetBlueprint, factory)
if not bp:
    raise RuntimeError("Unable to create party menu")
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_loaded_asset(bp)
unreal.log("PARTY_UI %s STATUS=%s" % (path, bp.status))
