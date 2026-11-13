import unreal

targets = [
    "/Game/UI/CraftingSystem/WBP_CraftingMenu",
    "/Game/UI/InventorySystem/WBP_NPCVendorShop",
    "/Game/UI/InventorySystem/WBP_NPCVendorShopSell",
    "/Game/UI/InventorySystem/WBP_VendorItemObject",
    "/Game/UI/World/WBP_Vendor",
]

for path in targets:
    asset = unreal.load_asset(path)
    if not asset:
        unreal.log_error("MISSING " + path)
        continue
    parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(asset)
    generated = asset.generated_class()
    unreal.log("COMMERCE %s PARENT=%s" % (path, parent.get_path_name() if parent else "None"))
    if generated:
        default = unreal.get_default_object(generated)
        for prop in unreal.get_editor_subsystem(unreal.EditorAssetSubsystem).get_metadata_tag_values(asset).keys():
            pass
        for name in ["Inventory", "InventoryComp", "CraftingComp", "Recipes", "Recipe", "Vendor", "Player", "PlayerCharacter"]:
            try:
                value = default.get_editor_property(name)
                unreal.log("  %s=%s" % (name, value))
            except Exception:
                pass
    unreal.BlueprintEditorLibrary.compile_blueprint(asset)
    unreal.log("  STATUS=%s" % asset.status)

