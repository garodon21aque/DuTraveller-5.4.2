import unreal


CONTROLLERS = (
    ("BP_RPGAttributeWidgetController", "/Script/DuTraveller.RPGAttributeMenuWidgetController"),
    ("BP_RPGOverlayWidgetController", "/Script/DuTraveller.RPGOverlayWidgetController"),
    ("BP_RPGSpellWidgetController", "/Script/DuTraveller.RPG_SpellMenuWidgetController"),
)

PACKAGE_PATH = "/Game/Blueprints/Controllers"
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

for asset_name, parent_path in CONTROLLERS:
    object_path = f"{PACKAGE_PATH}/{asset_name}.{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(object_path):
        unreal.log(f"Controller Blueprint already exists: {object_path}")
        continue

    parent_class = unreal.load_class(None, parent_path)
    if not parent_class:
        raise RuntimeError(f"Unable to load native parent class {parent_path}")

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    blueprint = asset_tools.create_asset(asset_name, PACKAGE_PATH, unreal.Blueprint, factory)
    if not blueprint:
        raise RuntimeError(f"Unable to create {object_path}")

    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False):
        raise RuntimeError(f"Unable to save {object_path}")

    unreal.log(f"Created controller Blueprint: {object_path} -> {parent_path}")
