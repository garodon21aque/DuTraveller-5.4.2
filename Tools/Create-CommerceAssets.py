import unreal

assets = unreal.AssetToolsHelpers.get_asset_tools()

def reparent(path, parent_path):
    bp = unreal.load_asset(path)
    parent = unreal.load_class(None, parent_path)
    if not bp or not parent:
        raise RuntimeError("Unable to load %s or %s" % (path, parent_path))
    unreal.BlueprintEditorLibrary.reparent_blueprint(bp, parent)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    unreal.log("COMMERCE_REPARENT %s -> %s STATUS=%s" % (path, parent_path, bp.status))

def blueprint(path, parent_path):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        bp = unreal.load_asset(path)
    else:
        package, name = path.rsplit("/", 1)
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.load_class(None, parent_path))
        bp = assets.create_asset(name, package, unreal.Blueprint, factory)
    if not bp:
        raise RuntimeError("Unable to create " + path)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    unreal.log("COMMERCE_BLUEPRINT %s STATUS=%s" % (path, bp.status))

reparent("/Game/UI/CraftingSystem/WBP_CraftingMenu", "/Script/DuTraveller.RPGCraftingMenuWidget")
reparent("/Game/UI/InventorySystem/WBP_NPCVendorShop", "/Script/DuTraveller.RPGVendorMenuWidget")
blueprint("/Game/Blueprints/Actors/Interactables/Vendors/BP_MasterVendor", "/Script/DuTraveller.RPGVendor")

